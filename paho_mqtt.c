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
int	xMqttRead(Network * psNetwork, u8_t * buffer, i16_t i16Len, u32_t mSecTime) {
	netx_t * psCtx = &psNetwork->sCtx;
	IF_EXEC(debugTRACK, psCtx->d.d = psCtx->d.r = OPT_GET(dbMQTTrw) & 1);
	int	iRV = xNetSetRecvTO(psCtx, mSecTime);
	if (iRV == erSUCCESS)
		iRV = xNetRecv(psCtx, buffer, i16Len);
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

/* ########## TEMPORARY DIAGNOSTIC - DELETE THIS BLOCK AND ITS ONE CALL WHEN DIAGNOSED ##########
 * Added 2026-08-12. Self-contained on purpose: this block + the single call below are the whole
 * change, and mqttTEMP_DISCARD_DIAG 0 compiles both to nothing.
 *
 * Why: messages on SiteWhere/commands/<mac> - the GMAP data path - intermittently reach the
 * DEFAULT handler and are dropped (c734 2026-08-05; c9c4 2026-08-11 22:33 lost ~22 IDENT blocks
 * in 2s). Handler PRE-BINDING (sitewhere b07ee65, in tag v0.6.1.11 which c9c4 was running) did
 * NOT fix it, and static analysis has now failed twice: MQTTConnectWithResults provably does not
 * wipe the handler table, and RX/TX cannot race (MQTT_TASK defined, both take sClient.mutex).
 * The one fact source-reading cannot supply is the table AS IT STANDS at the discard - so print it.
 *
 * Reading the output:
 *   slots=x00      table was emptied - find which MQTTCloseSession path did it (cycle/keepalive,
 *                  subscribe, unsubscribe, publish, disconnect - ALL wipe every slot, cleansession=1)
 *   slots non-zero handlers ARE bound, so deliverMessage's topic comparison is failing instead:
 *                  a different bug, and the search moves to isTopicMatched/MQTTPacket_equals
 *   con=0          delivery while disconnected, which should be unreachable
 *
 * Reported only when the state CHANGES, so a 22-message burst costs ONE line, not 22 (c764 lesson). */
#ifndef mqttTEMP_DISCARD_DIAG
	#define	mqttTEMP_DISCARD_DIAG	1			// TEMP: 0 = compiled out entirely
#endif

#if (mqttTEMP_DISCARD_DIAG > 0)
extern MQTTClient sClient;						// local extern: adds no #include, one block to delete

static void vMqttTempDiscardDiag(void) {
	static u8_t SigPrv = 0xFF;					// 0xFF = nothing reported yet
	static u32_t tLast = 0;
	u8_t Sig = sClient.isconnected ? 0x80 : 0;
	for (int i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
		if (sClient.messageHandlers[i].topicFilter)
			Sig |= (1 << i);
	/* A state CHANGE reports immediately; an UNCHANGED state still reports once a minute. A pure
	 * change-latch printed once per state per boot, so a second discard episode hours later was
	 * silent - the burst you are looking at would carry no diagnostic at all. A 22-message burst
	 * still costs ONE line. */
	u32_t tNow = xTaskGetTickCount();
	if (Sig == SigPrv && (tNow - tLast) < pdMS_TO_TICKS(60000))
		return;
	SigPrv = Sig;
	tLast = tNow;
	/* SL_ERR, not SL_WARN: this must clear the SAME severity gate as the DISCARDED line it
	 * explains (ERROR=3, WARNING=4, lower is more severe) or it is filtered out precisely when
	 * the thing it diagnoses is being logged. */
	SL_ERR("TEMPDIAG con=%d clean=%d slots=x%02X [0]'%s' [1]'%s' [2]'%s' [3]'%s' [4]'%s'",
		sClient.isconnected, sClient.cleansession, Sig & 0x1F,
		sClient.messageHandlers[0].topicFilter ?: "-", sClient.messageHandlers[1].topicFilter ?: "-",
		sClient.messageHandlers[2].topicFilter ?: "-", sClient.messageHandlers[3].topicFilter ?: "-",
		sClient.messageHandlers[4].topicFilter ?: "-");
}
#endif

void vMqttDefaultHandler(MessageData * psMD) {
	/* No handler matched = the message is being DISCARDED, say so. Payload as capped text AND
	 * capped hex: TB routes its (JSON) shared-attribute topic here by design, SW arrivals are
	 * protobuf - each form is readable for one and mojibake for the other. ID omitted: paho only
	 * populates it for QoS>0, so on QoS 0 it printed stale memory ("ID=16380", c734 2026-08-05). */
	#if (mqttTEMP_DISCARD_DIAG > 0)
	vMqttTempDiscardDiag();						// TEMP: delete with the block above
	#endif
	int Len = psMD->message->payloadlen;
	SL_ERR("DISCARDED QoS=%d  R=%d  D=%d  Topic='%.*s'  PL[%d]='%.*s' %!'+hhY",
		psMD->message->qos, psMD->message->retained, psMD->message->dup,
		psMD->topicName->lenstring.len, psMD->topicName->lenstring.data, Len,
		Len > 40 ? 40 : Len, psMD->message->payload,
		Len > 16 ? 16 : Len, psMD->message->payload);
}

#endif
