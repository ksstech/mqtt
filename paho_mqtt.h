// paho_mqtt.h

#pragma once

#include "printfx.h"
#include "socketsX.h"
#include "statistics.h"

#ifdef __cplusplus
extern "C" {
#endif

// ########################################### Macros ############################################

#define	MQTT_TASK				1

// ######################################## enumerations ###########################################


// ########################################## structures ###########################################

typedef struct { SemaphoreHandle_t sem; } Mutex;

typedef	struct { QueueHandle_t	queue; } Queue;

typedef struct { TaskHandle_t	task; } Thread;

typedef struct queue_publish_t {
	char *	pTopic;
    void *	pvPayload;
    size_t	xLoadlen;
} queue_publish_t;

typedef struct Timer {
	TickType_t	xTicksToWait;
	TimeOut_t	xTimeOut;
} Timer;

typedef struct Network Network;
struct Network {
	int (*mqttread) (Network * psNetwork, uint8_t *, int16_t, uint32_t mSecTime);
	int (*mqttwrite) (Network * psNetwork, uint8_t *, int16_t, uint32_t mSecTime);
	netx_t		sCtx;
	sock_sec_t	sSec;
};

// #################################### Public/global variables ####################################

extern const char * ccpPktType[];
extern volatile u8_t xMqttState;

#if (statsMQTT_RX > 0)
	extern x32mma_t * psMqttRX;
#endif

#if (statsMQTT_TX > 0)
	extern x32mma_t * psMqttTX;
#endif

// #################################### Public/global functions ####################################

struct MQTTClient;
int cycle(struct MQTTClient * c, Timer * t);		// helper function

void TimerCountdownMS(Timer * timer, unsigned int mSecTime);
void TimerCountdown(Timer * timer, unsigned int SecTime);
int	TimerLeftMS(Timer * timer);
char TimerIsExpired(Timer * timer);
void TimerInit(Timer * timer);

void MQTTNetworkInit(Network * psNetwork);
int	MQTTNetworkConnect(Network * psNetwork);
int ThreadStart(Thread * thread, void (*fn) (void *), void * arg);

void MutexInit(Mutex * mutex);
void MutexLock(Mutex * mutex);
void MutexUnlock(Mutex * mutex);

struct MessageData;
void vMqttDefaultHandler(struct MessageData * psMD);

#ifdef __cplusplus
}
#endif
