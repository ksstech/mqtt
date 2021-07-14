set( PPaho "${PRepo}/paho.mqtt.embedded-c" )

idf_component_register(
	SRCS "paho_mqtt.c"
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
	INCLUDE_DIRS "."
		"${PPaho}/MQTTClient-C/src"
		"${PPaho}/MQTTPacket/src"
	PRIV_REQUIRES printfx
	REQUIRES socketsX stringsX
)
