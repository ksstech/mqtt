idf_component_register(
	SRCS "paho_support.c" "paho.mqtt.embedded-c/MQTTClient-C/src/MQTTClient.c"
		 "paho.mqtt.embedded-c/MQTTPacket/src/MQTTConnectClient.c"
		 "paho.mqtt.embedded-c/MQTTPacket/src/MQTTConnectServer.c"
		 "paho.mqtt.embedded-c/MQTTPacket/src/MQTTDeserializePublish.c"
		 "paho.mqtt.embedded-c/MQTTPacket/src/MQTTFormat.c"
		 "paho.mqtt.embedded-c/MQTTPacket/src/MQTTPacket.c"
		 "paho.mqtt.embedded-c/MQTTPacket/src/MQTTSerializePublish.c"
		 "paho.mqtt.embedded-c/MQTTPacket/src/MQTTSubscribeClient.c"
		 "paho.mqtt.embedded-c/MQTTPacket/src/MQTTSubscribeServer.c"
		 "paho.mqtt.embedded-c/MQTTPacket/src/MQTTUnsubscribeClient.c"
		 "paho.mqtt.embedded-c/MQTTPacket/src/MQTTUnsubscribeServer.c"
	INCLUDE_DIRS "." "paho.mqtt.embedded-c/MQTTClient-C/src" "paho.mqtt.embedded-c/MQTTPacket/src"
	REQUIRES socketsX
)
