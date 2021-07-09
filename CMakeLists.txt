set( PP "~/Dropbox/devs/ExRepo/paho.mqtt.embedded-c" )

idf_component_register(
	SRCS "paho_mqtt.c"
		"${PP}/MQTTClient-C/src/MQTTClient.c"
		"${PP}/MQTTPacket/src/MQTTConnectClient.c"
		"${PP}/MQTTPacket/src/MQTTConnectServer.c"
		"${PP}/MQTTPacket/src/MQTTDeserializePublish.c"
		"${PP}/MQTTPacket/src/MQTTFormat.c"
		"${PP}/MQTTPacket/src/MQTTPacket.c"
		"${PP}/MQTTPacket/src/MQTTSerializePublish.c"
		"${PP}/MQTTPacket/src/MQTTSubscribeClient.c"
		"${PP}/MQTTPacket/src/MQTTSubscribeServer.c"
		"${PP}/MQTTPacket/src/MQTTUnsubscribeClient.c"
		"${PP}/MQTTPacket/src/MQTTUnsubscribeServer.c"
	INCLUDE_DIRS "."
		"${PP}/MQTTClient-C/src"
		"${PP}/MQTTPacket/src"
	PRIV_REQUIRES printfx
	REQUIRES socketsX stringsX
)
