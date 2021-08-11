/*
 * paho_mqtt.h
 */

#pragma		once

#include	"socketsX.h"								// x_ubuf FreeRTOS_Support LwIP mbedTLS
#include	"commands.h"

//#include	<stddef.h>
//#include	<stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ########################################### Macros ############################################


// ######################################## enumerations ###########################################

enum {
	stateMQTT_INVALID,
	stateMQTT_RESOLVING,
	stateMQTT_CONNECTING,
	stateMQTT_SUBSCRIBING,
	stateMQTT_RUNNING,
	stateMQTT_STOP,
	stateMQTT_DISCONNECT,
	stateMQTT_CLOSE,
} ;

// ########################################## structures ###########################################

typedef struct { SemaphoreHandle_t sem ; } Mutex ;

typedef	struct { QueueHandle_t	queue; } Queue ;

typedef struct { TaskHandle_t	task; } Thread ;

typedef struct sQueuePublish {
	char *	pTopic ;
    void *	pvPayload ;
    size_t	xLoadlen;
} sQueuePublish ;

typedef struct Timer {
	TickType_t	xTicksToWait ;
	TimeOut_t	xTimeOut ;
} Timer ;

typedef struct Network Network ;
struct Network {
	int (*mqttread) (Network * psNetwork, uint8_t *, int16_t, uint32_t mSecTime) ;
	int (*mqttwrite) (Network * psNetwork, uint8_t *, int16_t, uint32_t mSecTime) ;
	netx_t		sCtx ;
	sock_sec_t	sSec ;
};

// #################################### Public/global variables ####################################

extern uint8_t	xMqttState ;

// #################################### Public/global functions ####################################

void TimerCountdownMS(Timer * timer, uint32_t mSecTime) ;
void TimerCountdown(Timer * timer, uint32_t SecTime) ;
int	TimerLeftMS(Timer * timer) ;
char TimerIsExpired(Timer * timer) ;
void TimerInit(Timer * timer) ;

void MQTTNetworkInit(Network * psNetwork) ;
int	MQTTNetworkConnect(Network * psNetwork) ;
int ThreadStart(Thread * thread, void (*fn) (void *), void * arg) ;

void MutexInit(Mutex * mutex) ;
void MutexLock(Mutex * mutex) ;
void MutexUnlock(Mutex * mutex) ;

struct MessageData ;
void vMqttDefaultHandler(struct MessageData * psMD) ;
int CmndMQTT(cli_t * psCLI) ;

#ifdef __cplusplus
}
#endif
