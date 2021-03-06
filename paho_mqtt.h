/*
 * paho_mqtt.h
 */

#pragma		once

#if 1
	#include	"FreeRTOS_Support.h"
#else
	#include	"freertos/FreeRTOS.h"
	#include	"freertos/semphr.h"
	#include	"freertos/queue.h"
	#include	"freertos/task.h"
	#include	"freertos/portmacro.h"
#endif

#include	"socketsX.h"								// hal_config + LwIP + mbedTLS
#include	"commands.h"

#ifdef __cplusplus
extern "C" {
#endif

// ########################################### Macros ############################################

#define	MQTT_TASK									// enable basic 2nd task functionality

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

typedef struct {
	TickType_t	xTicksToWait ;
	TimeOut_t	xTimeOut ;
} Timer ;

typedef struct network_s Network ;

struct network_s {
	int (*mqttread) (Network * psNetwork, uint8_t *, int16_t, uint32_t mSecTime) ;
	int (*mqttwrite) (Network * psNetwork, uint8_t *, int16_t, uint32_t mSecTime) ;
	netx_t		sCtx ;
	sock_sec_t	sSec ;
} ;

typedef struct { SemaphoreHandle_t	sem ; } Mutex ;

typedef	struct { QueueHandle_t	queue ; } Queue ;

typedef struct { TaskHandle_t	task ; } Thread ;

typedef struct	sQueuePublish {
	char *	pTopic ;
    void *	pvPayload ;
    size_t	xLoadlen;
} sQueuePublish ;

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

int CmndMQTT(cli_t * psCLI) ;
struct MessageData ;
void vMqttDefaultHandler(struct MessageData * psMD) ;

#ifdef __cplusplus
}
#endif

#include	"MQTTClient.h"
