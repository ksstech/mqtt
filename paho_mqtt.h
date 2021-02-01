/*
 * paho_support.h
 */

#pragma		once

#include	"socketsX.h"								// hal_config + LwIP + mbedTLS

#include	"freertos/FreeRTOS.h"
#include	"freertos/semphr.h"
#include	"freertos/queue.h"
#include	"freertos/task.h"
#include	"freertos/portmacro.h"

// ########################################### Macros ############################################

#define	MQTT_TASK									// enable basic 2nd task functionality

// ######################################## enumerations ###########################################


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

// #################################### Public/global variables ####################################


// #################################### Public/global functions ####################################

void	TimerCountdownMS(Timer * timer, uint32_t mSecTime) ;
void	TimerCountdown(Timer * timer, uint32_t SecTime) ;
int		TimerLeftMS(Timer * timer) ;
char	TimerIsExpired(Timer * timer) ;
void	TimerInit(Timer * timer) ;

void	MQTTNetworkInit(Network * psNetwork) ;
int		MQTTNetworkConnect(Network * psNetwork, const char * addr, sock_sec_t * psSec) ;

int 	ThreadStart(Thread * thread, void (*fn) (void *), void * arg) ;

void	MutexInit(Mutex * mutex) ;
void 	MutexLock(Mutex * mutex) ;
void	MutexUnlock(Mutex * mutex) ;
