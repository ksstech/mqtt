/*
 * paho_mqtt.c
 */

/*
MQTT basic call chart:
----------------------

MQTTUnsubscribe		->	waitfor
MQTTSubcribe		->	waitfor
MQTTPublish			->	waitfor
MQTTConnect			->	waitfor
						waitfor				->	cycle											/>	decodePacket		-> mqttread
						vMqttTaskRx			->	cycle				->	readPacket				->	mqttread
																	|
MQTTStartTask		->	MQTTRun	(unused)	->	cycle
						MqttYield (unused)	->	cycle
																	|
																	V
						vMqttTaskRx			->	keepalive			->	sendPacket
						vMqttInitNetworConn	->	MQTTConnect			->	sendPacket
												MQTTDisconnet		->	sendPacket
						vMqtt_TaskTx		->	MQTTPublish			->	sendPacket
						MQTTInitSubscribe	->	MQTTSubscribe		->	sendPacket
												MQTTUnsubscribe		->	sendPacket				->	MutexLock
																								->	mqttwrite
===== Changes required =====
Add following line at end of MQTTClient.h, before #endif
	int cycle(MQTTClient* c, Timer* timer) ;
*/

#include "hal_variables.h"

#include "MQTTClient.h"
#include "options.h"
#include "printfx.h"
#include "statistics.h"
#include "syslog.h"
#include "x_errors_events.h"
#include "x_string_to_values.h"
#include "x_time.h"

#define	debugFLAG					0x0000

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// #################################### Public/global variables ####################################

char MQTTHostName[sizeof("000.000.000.000")];
volatile u8_t xMqttState;

#if (statsMQTT_RX > 0)
	x32mma_t * psMqttRX;
#endif

#if (statsMQTT_TX > 0)
	x32mma_t * psMqttTX;
#endif

// #################################### Public/global functions ####################################

void TimerCountdownMS(Timer * timer, unsigned int mSecTime) {
	timer->xTicksToWait = pdMS_TO_TICKS(mSecTime) ;		// milliseconds to ticks
	vTaskSetTimeOutState(&timer->xTimeOut) ; 			// Record the time function entered.
}

void TimerCountdown(Timer * timer, unsigned int SecTime) { TimerCountdownMS(timer, SecTime * 1000); }

int	TimerLeftMS(Timer * timer) {
	xTaskCheckForTimeOut(&timer->xTimeOut, &timer->xTicksToWait) ;
	return timer->xTicksToWait * portTICK_PERIOD_MS ;
}

char TimerIsExpired(Timer * timer) {
	return (xTaskCheckForTimeOut(&timer->xTimeOut, &timer->xTicksToWait) == pdTRUE);
}

void TimerInit(Timer * timer) { memset(timer, 0, sizeof(Timer)); }

/**
 * network_read() -
 * @param	psNetwork
 * @param	buffer
 * @param	i16Len
 * @param	mSecTime
 * @return	By default the number of bytes read (>0)
 * 			Timeout (0)
 * 			Error (<0)
 */
int	network_read(Network * psNetwork, u8_t * buffer, s16_t i16Len, u32_t mSecTime) {
	int	iRV = xNetSetRecvTO(&psNetwork->sCtx, mSecTime);
	if (iRV == erSUCCESS)
		iRV = xNetRecv(&psNetwork->sCtx, buffer, i16Len);
	if (iRV == i16Len) {
		IF_EXEC_2(statsMQTT_RX > 0, x32MMAupdate, psMqttRX, (x32_t)iRV);
		return iRV;
	}
	// paho does not want to know about EAGAIN, filter out and return 0...
	return (iRV < 0 && psNetwork->sCtx.error == EAGAIN) ? 0 : iRV;
}

int	network_write(Network * psNetwork, u8_t * buffer, s16_t i16Len, u32_t mSecTime) {
	psNetwork->sCtx.tOut = mSecTime ;
	int iRV = xNetSelect(&psNetwork->sCtx, selFLAG_WRITE) ;
	if (iRV > erSUCCESS)
		iRV = xNetSend(&psNetwork->sCtx, buffer, i16Len);
	IF_EXEC_2(statsMQTT_TX > 0 && iRV == i16Len, x32MMAupdate, psMqttTX, (x32_t)iRV);
	return iRV ;
}

void MQTTNetworkInit(Network * psNetwork) {
	psNetwork->sCtx.sd = -1;
	psNetwork->mqttread = network_read;
	psNetwork->mqttwrite = network_write;
	IF_EXEC_1(statsMQTT_RX > 0, psMqttRX = px32MMAinit, vfUXX);
	IF_EXEC_1(statsMQTT_TX > 0, psMqttTX = px32MMAinit, vfUXX);
}

int	MQTTNetworkConnect(Network * psNetwork) {
	memset(&psNetwork->sCtx, 0 , sizeof(netx_t)) ;
	psNetwork->sCtx.type = SOCK_STREAM;
	psNetwork->sCtx.sa_in.sin_family= AF_INET;
	psNetwork->sCtx.flags = SO_REUSEADDR;
	if (nvsWifi.ipMQTT) {						// MQTT broker specified
		snprintfx(MQTTHostName, sizeof(MQTTHostName), "%#-I", nvsWifi.ipMQTT);
		psNetwork->sCtx.pHost = MQTTHostName;
	} else {									// default cloud MQTT host
		psNetwork->sCtx.pHost = HostInfo[ioB2GET(ioHostMQTT)].pName;
	}
	if (nvsWifi.ipMQTTport) {
		psNetwork->sCtx.sa_in.sin_port = htons(nvsWifi.ipMQTTport);
	} else {
		psNetwork->sCtx.sa_in.sin_port = htons(IP_PORT_MQTT + (10000 * ioB2GET(ioMQTTport)));
	}
	if (debugTRACK && ioB1GET(ioMQcon)) {
		SL_NOT("Using MQTT broker %s:%hu\r\n", psNetwork->sCtx.pHost, psNetwork->sCtx.sa_in.sin_port);
		psNetwork->sCtx.d = (union netx_dbg_u) { .o=1, .h=1, .bl=1, .t=1, .a=1, .s=1, .w=1, .r=1, .d=1 };
	}
	return xNetOpen(&psNetwork->sCtx);
}

/*
 * ThreadStart()
 * \brief		Unused, just for compatibility to minimise changes to standard library
 */
int ThreadStart(Thread * thread, void (*fn)(void *), void * arg) {
	int rc = 0;
	u16_t usTaskStackSize = (configMINIMAL_STACK_SIZE * 5);
	UBaseType_t uxTaskPriority = uxTaskPriorityGet(NULL); /* set the priority as the same as the calling task*/

	rc = xTaskCreate(fn,	/* The function that implements the task. */
		"MQTTTask",			/* Just a text name for the task to aid debugging. */
		usTaskStackSize,	/* The stack size is defined in FreeRTOSIPConfig.h. */
		arg,				/* The task parameter, not used in this case. */
		uxTaskPriority,		/* The priority assigned to the task is defined in FreeRTOSConfig.h. */
		&thread->task);		/* The task handle is not used. */

	return rc;
}

void MutexInit(Mutex * mutex)	{ vRtosSemaphoreInit(&mutex->sem); }

void MutexLock(Mutex * mutex)	{ xRtosSemaphoreTake(&mutex->sem, portMAX_DELAY); }

void MutexUnlock(Mutex * mutex)	{ xRtosSemaphoreGive(&mutex->sem); }

void vMqttDefaultHandler(MessageData * psMD) {
	SL_ERR("QoS=%d  Retained=%d  Dup=%d  ID=%d  Topic='%.*s'  PL='%.*s'",
		psMD->message->qos, psMD->message->retained,psMD->message->dup, psMD->message->id,
		psMD->topicName->lenstring.len, psMD->topicName->lenstring.data,
		psMD->message->payloadlen, psMD->message->payload) ;
}
