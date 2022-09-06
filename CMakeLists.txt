# PAHO MQTT

set( PPaho "${PRepo}/paho.mqtt.embedded-c" )

set( srcs "paho_mqtt.c"
		"${PPaho}/MQTTClient-C/src/MQTTClient.c"
		"${PPaho}/MQTTPacket/src/MQTTConnectClient.c"
		"${PPaho}/MQTTPacket/src/MQTTConnectServer.c"
		"${PPaho}/MQTTPacket/src/MQTTDeserializePublish.c"
		"${PPaho}/MQTTPacket/src/MQTTFormat.c"
		"${PPaho}/MQTTPacket/src/MQTTPacket.c"
		"${PPaho}/MQTTPacket/src/MQTTSerializePublish.c"
		"${PPaho}/MQTTPacket/src/MQTTSubscribeClient.c"
		"${PPaho}/MQTTPacket/src/MQTTSubscribeServer.c"
		"${PPaho}/MQTTPacket/src/MQTTUnsubscribeClient.c"
		"${PPaho}/MQTTPacket/src/MQTTUnsubscribeServer.c" 
)

set( include_dirs "." "${PPaho}/MQTTClient-C/src" "${PPaho}/MQTTPacket/src" )
set( priv_include_dirs )
set( requires commands rtos-support socketsX )
set( priv_requires common hal_esp32 printfx stringsX syslog )

list( APPEND requires identity sitewhere thingsboard )

idf_component_register(
	SRCS ${srcs} 
	INCLUDE_DIRS ${include_dirs} 
	PRIV_INCLUDE_DIRS ${priv_include_dirs} 
	REQUIRES ${requires} 
	PRIV_REQUIRES ${priv_requires}
)
