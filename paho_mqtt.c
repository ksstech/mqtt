// paho_mqtt.c - Copyright (c) 2017-25 Andre M. Maree / KSS Technologies (Pty) Ltd.

/*
MQTT basic call chart:
----------------------

MQTTUnsubscribe		->	waitfor
MQTTSubcribe		->	waitfor
MQTTPublish			->	waitfor
MQTTConnect			->	waitfor
						waitfor				->	cycle											->	decodePacket		-> mqttread
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
	int cycle(MQTTClient* c, Timer* timer);
*/

#include "hal_platform.h"

#if (appAEP > 0)
#include "hal_network.h"
#include "hal_options.h"
#include "certificates.h"
#include "MQTTClient.h"
#include "printfx.h"
#include "statistics.h"
#include "syslog.h"
#include "errors_events.h"
#include "string_to_values.h"
#include "timeX.h"

#define	debugFLAG					0xF000

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// #################################### Public/global constants ####################################

const char * ccpPktType[] = {
	"BUffer", "Error", "TimeOut", "Connect", "CONack", "PUBLISH", "PUBack", "PUBREC", "PUBREL",
	"PUBCOMP", "SUBSCRIBE", "SUBack", "UNSUBSCRIBE", "UNSUBack", "PINGREQ", "PINGRESP", "DISCONNECT"
};

// #################################### Public/global variables ####################################

char MQTTHostName[sizeof("000.000.000.000")];
volatile u8_t xMqttState;

x32mma_t *psMqttRX, *psMqttTX;

// #################################### Public/global functions ####################################

void TimerCountdownMS(Timer * timer, unsigned int mSecTime) {
	timer->xTicksToWait = pdMS_TO_TICKS(mSecTime);		// milliseconds to ticks
	vTaskSetTimeOutState(&timer->xTimeOut); 			// Record the time function entered.
}

void TimerCountdown(Timer * timer, unsigned int SecTime) { TimerCountdownMS(timer, SecTime * 1000); }

int	TimerLeftMS(Timer * timer) {
	xTaskCheckForTimeOut(&timer->xTimeOut, &timer->xTicksToWait);
	return timer->xTicksToWait * portTICK_PERIOD_MS;
}

char TimerIsExpired(Timer * timer) {
	return (xTaskCheckForTimeOut(&timer->xTimeOut, &timer->xTicksToWait) == pdTRUE);
}

void TimerInit(Timer * timer) { memset(timer, 0, sizeof(Timer)); }

/**
 * @brief		Unused, just for compatibility to minimise changes to standard library
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

void MutexInit(Mutex * mutex)	{ xRtosSemaphoreInit(&mutex->sem); }

void MutexLock(Mutex * mutex)	{ xRtosSemaphoreTake(&mutex->sem, portMAX_DELAY); }

void MutexUnlock(Mutex * mutex)	{ xRtosSemaphoreGive(&mutex->sem); }

/**
 * @brief		read data from the MQTT host
 * @param[in]	psNetwork
 * @param[in]	buffer
 * @param[in]	i16Len
 * @param[in]	mSecTime
 * @return	Number of bytes read (>0), Timeout (0) or Error (<0)
 */
int	xMqttRead(Network * psNetwork, u8_t * buffer, i16_t i16Len, u32_t mSecTime) {
	netx_t * psCtx = &psNetwork->sCtx;
	IF_EXEC(debugTRACK, psCtx->d.d = psCtx->d.r = xOptionGet(dbMQTTrw) & 1);
	int	iRV = xNetSetRecvTO(psCtx, mSecTime);
	if (iRV == erSUCCESS)
		iRV = xNetRecv(psCtx, buffer, i16Len);
	if (iRV == i16Len)
		return iRV;
	// paho does not want to know about EAGAIN, filter out and return 0...
	return (iRV < 0 && psCtx->error == EAGAIN) ? 0 : iRV;
}

/**
 * @brief		write data to the MQTT host
 * @param[in]	psNetwork
 * @param[in]	buffer
 * @param[in]	i16Len
 * @param[in]	mSecTime
 * @return	Number of bytes written (>0), Timeout (0) or Error (<0)
 */
int	xMqttWrite(Network * psNetwork, u8_t * buffer, i16_t i16Len, u32_t mSecTime) {
	netx_t * psCtx = &psNetwork->sCtx;
	IF_EXEC(debugTRACK, psCtx->d.d = psCtx->d.w = xOptionGet(dbMQTTrw) & 2 ? 1 : 0);
	psCtx->tOut = mSecTime;
	int iRV = xNetSelect(psCtx, selFLAG_WRITE);
	if (iRV > erSUCCESS)
		iRV = xNetSend(psCtx, buffer, i16Len);
	return iRV;
}

void vMqttNetworkInit(Network * psNetwork) {
	psNetwork->sCtx.sd = -1;
	psNetwork->mqttread = xMqttRead;
	psNetwork->mqttwrite = xMqttWrite;
}

int xMqttNetworkConnect(netx_t * psCtx) {
	memset(psCtx, 0 , sizeof(netx_t));
	psCtx->type = SOCK_STREAM;
	psCtx->flags = SO_REUSEADDR;
	psCtx->sa_in.sin_family= AF_INET;
	if (nvsWifi.ipMQTT) {								// MQTT broker specified
		snprintfx(MQTTHostName, sizeof(MQTTHostName), "%#-I", nvsWifi.ipMQTT);
		psCtx->pHost = MQTTHostName;
	} else {											// default cloud MQTT host
		psCtx->pHost = HostInfo[xOptionGet(ioHostMQTT)].pName;
	}
	psCtx->sa_in.sin_port = htons(nvsWifi.ipMQTTport ? nvsWifi.ipMQTTport : IP_PORT_MQTT + (10000 * xOptionGet(ioMQTTport)));
#if (appRECONNECT > 0)
	psCtx->ReConnect = 3;								/* Add flag to enable auto reconnect */
#endif
	if (debugTRACK && xOptionGet(ioMQcon))
		SL_NOT("Using MQTT broker %s:%hu", psCtx->pHost, ntohs(psCtx->sa_in.sin_port));
	return xNetOpen(psCtx);
}

void vMqttDefaultHandler(MessageData * psMD) {
	SL_ERR("QoS=%d  Retained=%d  Dup=%d  ID=%d  Topic='%.*s'  PL='%.*s'",
		psMD->message->qos, psMD->message->retained,psMD->message->dup, psMD->message->id,
		psMD->topicName->lenstring.len, psMD->topicName->lenstring.data,
		psMD->message->payloadlen, psMD->message->payload);
}

#endif
