/*
 * paho_support.h
 */

#pragma		once

#include	"MQTTClient.h"

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


// #################################### Public/global variables ####################################


// #################################### Public/global functions ####################################

void	vMqttDefaultHandler(MessageData * psMD) ;

void	MQTTNetworkInit(Network * psNetwork) ;
int		MQTTNetworkConnect(Network * psNetwork, const char * addr, sock_sec_t * psSec) ;

#ifdef __cplusplus
extern }
#endif

