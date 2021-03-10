set( PP "paho.mqtt.embedded-c/MQTTPacket/src" )

idf_component_register(
	SRCS "paho_support.c"
		"paho.mqtt.embedded-c/MQTTClient-C/src/MQTTClient.c"
		"${PP}/MQTTConnectClient.c"
		"${PP}/MQTTConnectServer.c"
		"${PP}/MQTTDeserializePublish.c"
		"${PP}/MQTTFormat.c"
		"${PP}/MQTTPacket.c"
		"${PP}/MQTTSerializePublish.c"
		"${PP}/MQTTSubscribeClient.c"
		"${PP}/MQTTSubscribeServer.c"
		"${PP}/MQTTUnsubscribeClient.c"
		"${PP}/MQTTUnsubscribeServer.c"
	INCLUDE_DIRS "."
		"paho.mqtt.embedded-c/MQTTClient-C/src"
		"${PP}"
	PRIV_REQUIRES printfx
	REQUIRES socketsX
)
