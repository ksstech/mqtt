// paho_mqtt.c - Copyright (c) 2017-26 Andre M. Maree / KSS Technologies (Pty) Ltd.

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
===== Vendored tree =====
paho.mqtt.embedded-c is a pristine upstream clone and must stay that way.
Everything platform specific enters through MQTTCLIENT_PLATFORM_HEADER=paho_mqtt.h: Timer, Network,
Mutex, Thread, MQTT_TASK and the cycle() prototype (cycle is non-static upstream, no header edit).
*/

#include "hal_platform.h"

#if (cmakeAEP > 0)
#include "hal_network.h"
#include "certificates.h"
#include "MQTTClient.h"
#include "options.h"
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
/* readPacket() assumes mqttread() returns ALL i16Len bytes or fails. On a short read it abandons
 * the packet and leaves the remainder in the socket, so the NEXT header is decoded from mid-payload
 * - garbage rem_len, BUFFER_OVERFLOW, session close. xNetRecv is a single recv() and POSIX allows
 * it to return less, so loop to completion here. xNetRecvBlocks() is NOT usable: the socketsX block
 * section is behind appUSE_SOCKET_LOCKS, which is defined nowhere and never built. */
#define	mqttREAD_REST_MS	1000					// bound the wait for the REST of an arriving packet

int	xMqttRead(Network * psNetwork, u8_t * buffer, i16_t i16Len, u32_t mSecTime) {
	netx_t * psCtx = &psNetwork->sCtx;
	IF_EXEC(debugTRACK, psCtx->d.d = psCtx->d.r = OPT_GET(dbMQTTrw) & 1);
	int	iRV = xNetSetRecvTO(psCtx, mSecTime);
	if (iRV != erSUCCESS)
		return iRV;
	int xDone = 0;
	while (xDone < i16Len) {
		iRV = xNetRecv(psCtx, buffer + xDone, i16Len - xDone);
		if (iRV < 1)								// 0 = peer closed, <0 = error or EAGAIN timeout
			break;
		xDone += iRV;
		if (xDone < i16Len)							// packet mid-flight, cap the wait for the rest
			xNetSetRecvTO(psCtx, mqttREAD_REST_MS);
	}
	if (xDone == i16Len)
		return xDone;								// complete: the ONLY result readPacket accepts
	if (xDone > 0)									// desynced, and this used to be entirely silent
		SL_ERR("MQTT short read %d of %hd", xDone, i16Len);
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
	IF_EXEC(debugTRACK, psCtx->d.d = psCtx->d.w = OPT_GET(dbMQTTrw) & 2 ? 1 : 0);
	psCtx->tOut = mSecTime;
	return xNetSend(psCtx, buffer, i16Len);
}

void vMqttNetworkInit(Network * psNetwork) {
	psNetwork->sCtx.sd = -1;
	psNetwork->mqttread = xMqttRead;
	psNetwork->mqttwrite = xMqttWrite;
}

int xMqttNetworkConnect(netx_t * psCtx) {
	memset(psCtx, 0 , sizeof(netx_t));
	psCtx->c.type = SOCK_STREAM;
	psCtx->flags = SO_REUSEADDR;
	psCtx->sa_in.sin_family= AF_INET;
	if (nvsWifi.ipMQTT) {								// MQTT broker specified
		snprintfx(MQTTHostName, sizeof(MQTTHostName), "%#-I", nvsWifi.ipMQTT);
		psCtx->pHost = MQTTHostName;
	} else {											// default cloud MQTT host
		psCtx->pHost = HostInfo[OPT_GET(ioHostMQTT)].pName;
	}
	psCtx->sa_in.sin_port = htons(nvsWifi.ipMQTTport ? nvsWifi.ipMQTTport : IP_PORT_MQTT + (10000 * OPT_GET(ioMQTTport)));
#if (appRECONNECT > 0)
	psCtx->c.RCmax = 3;									/* Add flag to enable auto reconnect */
#endif
	if (debugTRACK && OPT_GET(ioMQcon))
		SL_NOT("Using MQTT broker %s:%hu", psCtx->pHost, ntohs(psCtx->sa_in.sin_port));
	return xNetOpen(psCtx);
}

void vMqttDefaultHandler(MessageData * psMD) {
	/* No handler matched = the message is being DISCARDED, say so. Payload as capped text AND
	 * capped hex: TB routes its (JSON) shared-attribute topic here by design, SW arrivals are
	 * protobuf - each form is readable for one and mojibake for the other. ID omitted: paho only
	 * populates it for QoS>0, so on QoS 0 it printed stale memory ("ID=16380", c734 2026-08-05). */
	int Len = psMD->message->payloadlen;
	SL_ERR("DISCARDED QoS=%d  R=%d  D=%d  Topic='%.*s'  PL[%d]='%.*s' %!'+hhY",
		psMD->message->qos, psMD->message->retained, psMD->message->dup,
		psMD->topicName->lenstring.len, psMD->topicName->lenstring.data, Len,
		Len > 40 ? 40 : Len, psMD->message->payload,
		Len > 16 ? 16 : Len, psMD->message->payload);
}

#endif
