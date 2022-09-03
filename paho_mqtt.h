/*
 * paho_mqtt.h
 */

#pragma		once

#include	"socketsX.h"					// x_ubuf FreeRTOS_Support LwIP mbedTLS
#include	"printfx.h"

#if (halUSE_AEP == 1)
	#include "task_sitewhere.h"
	#include "sitewhere-gpb.h"
	#if	(configUSE_IDENT > 0)
	#include "ident1.h"
	#include "identity.h"
	#endif
#elif (halUSE_AEP == 2)
	#include "task_thingsboard.h"
	#include "tb_json.h"
	#if	(configUSE_IDENT > 0)
	#include "ident2.h"
	#include "identity.h"
	#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ########################################### Macros ############################################

#define 	MQTT_TASK				1

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

extern volatile uint8_t	xMqttState;

// #################################### Public/global functions ####################################

void TimerCountdownMS(Timer * timer, unsigned int mSecTime);
void TimerCountdown(Timer * timer, unsigned int SecTime);
int	TimerLeftMS(Timer * timer);
char TimerIsExpired(Timer * timer);
void TimerInit(Timer * timer);

void MQTTNetworkInit(Network * psNetwork) ;
int	MQTTNetworkConnect(Network * psNetwork) ;
int ThreadStart(Thread * thread, void (*fn) (void *), void * arg) ;

void MutexInit(Mutex * mutex) ;
void MutexLock(Mutex * mutex) ;
void MutexUnlock(Mutex * mutex) ;

struct MessageData ;
void vMqttDefaultHandler(struct MessageData * psMD) ;

void vAEP_ReRegister(void);
void vAEP_Start(void);
void vAEP_Report(void);

#ifdef __cplusplus
}
#endif
