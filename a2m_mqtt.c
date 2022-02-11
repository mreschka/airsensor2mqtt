#include <mosquitto.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "a2m_mqtt.h"
#include "cmdline.h"

struct mosquitto* mosq = NULL;
char* mqttTopic = NULL;
char* mqttConTopic = NULL;

void a2m_mqtt_connect_callback(struct mosquitto* mosq, void* obj, int result);
void a2m_mqtt_message_callback(struct mosquitto* mosq, void* obj, const struct mosquitto_message* message);
void a2m_mqtt_log_callback(struct mosquitto* mosq, void* userdata, int level, const char* str);

bool a2m_mqtt_setup(struct gengetopt_args_info args_info) {

	int keepalive = 60;
	uint8_t clean_session = true;
	int ret = 0;

	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: MQTT: Init");
	mosquitto_lib_init();
	mosq = mosquitto_new(args_info.sensor_name_arg, clean_session, NULL);
	if (!mosq) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: MQTT: Out of memory.");
		return false;
	}

	mosquitto_connect_callback_set(mosq, a2m_mqtt_connect_callback);
	mosquitto_message_callback_set(mosq, a2m_mqtt_message_callback);
	mosquitto_log_callback_set(mosq, a2m_mqtt_log_callback);


	if (args_info.mqtt_username_given) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: MQTT: Set User: '%s' & PW '***'", args_info.mqtt_username_arg);
		ret = mosquitto_username_pw_set(mosq, args_info.mqtt_username_arg, args_info.mqtt_password_arg);
		if (ret) {
			syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: MQTT: Unable to set user and passwd: %s.", mosquitto_strerror(ret));
			return false;
		}
	}

	mqttConTopic = malloc(strlen(args_info.mqtt_base_name_arg) + strlen("/connected") + 1);
	strcpy(mqttConTopic, args_info.mqtt_base_name_arg);
	strcat(mqttConTopic, "/connected");
	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: MQTT: Connection Topic: %s", mqttConTopic);

	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: MQTT: Set will");
	ret = mosquitto_will_set(mosq, mqttConTopic, strlen("0"), "0", 0, 1);
	if (ret) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_WARNING), "WARNING: MQTT: Unable to set will: %s", mosquitto_strerror(ret));
		return false;
	}

	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: MQTT: Set Server '%s', port '%d', keepalive '%d'", args_info.mqtt_server_arg, args_info.mqtt_server_port_arg, keepalive);
	ret = mosquitto_connect(mosq, args_info.mqtt_server_arg, args_info.mqtt_server_port_arg, keepalive);
	if (ret) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: MQTT: Unable to connect: %s", mosquitto_strerror(ret));
		return false;
	}

	/*
		syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: MQTT: Start loop");
		int ret = mosquitto_subscribe(mosq, NULL, "/devices/wb-adc/controls/+", 0);
		mosquitto_strerror(ret)
	*/

	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: MQTT: Start loop");
	ret = mosquitto_loop_start(mosq);
	if (ret != MOSQ_ERR_SUCCESS) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: MQTT: Unable to start loop: %s", mosquitto_strerror(ret));
		return false;
	}

	a2m_mqtt_publish_connect("1");

	mqttTopic = malloc(strlen(args_info.mqtt_base_name_arg) + strlen("/status/") + strlen(args_info.sensor_name_arg) + 1);
	strcpy(mqttTopic, args_info.mqtt_base_name_arg);
	strcat(mqttTopic, "/status/");
	strcat(mqttTopic, args_info.sensor_name_arg);
	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: MQTT: Status Topic: %s", mqttTopic);
	return true;
}

void a2m_mqtt_close() {
	if (mosq != NULL) {
		a2m_mqtt_publish_connect("0");
		syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: MQTT Closing connection.");
		mosquitto_disconnect(mosq);
		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
	}
}

void a2m_mqtt_log_callback(struct mosquitto* mosq, void* userdata, int level, const char* str)
{
	switch (level) {
	case MOSQ_LOG_DEBUG:
		syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: MQTT: %s", str);
		break;
	case MOSQ_LOG_INFO:
		syslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), "INFO: MQTT: %s", str);
		break;
	case MOSQ_LOG_NOTICE:
		syslog(LOG_MAKEPRI(LOG_USER, LOG_NOTICE), "NOTICE: MQTT: %s", str);
		break;
	case MOSQ_LOG_WARNING:
		syslog(LOG_MAKEPRI(LOG_USER, LOG_WARNING), "WARNING: MQTT: %s", str);
		break;
	case MOSQ_LOG_ERR:
		syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: MQTT: %s", str);
		break;
	}
}

int a2m_mqtt_publish_connect(char* msg) {
	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: MQTT publish: %s %s", mqttConTopic, msg);
	int ret = mosquitto_publish(mosq, NULL, mqttConTopic, strlen(msg), msg, 0, 1);
	if (ret != MOSQ_ERR_SUCCESS)
		syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: MQTT publish: %s", mosquitto_strerror(ret));
	return ret;
}

int a2m_mqtt_publish(char* msg) {
	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: MQTT publish: %s %s", mqttTopic, msg);
	int ret = mosquitto_publish(mosq, NULL, mqttTopic, strlen(msg), msg, 0, 0);
	if (ret != MOSQ_ERR_SUCCESS)
		syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: MQTT publish: %s", mosquitto_strerror(ret));
	return ret;
}

void a2m_mqtt_connect_callback(struct mosquitto* mosq, void* obj, int result)
{
	switch (result) {
	case 0:// - success
		syslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), "INFO: MQTT connect callback: connected: %s", mosquitto_connack_string(result));
		break;
	default:
		syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: MQTT connect callback: %s", mosquitto_connack_string(result));
		break;
	}
}

void a2m_mqtt_message_callback(struct mosquitto* mosq, void* obj, const struct mosquitto_message* message)
{
	//bool match = 0;
	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: MQTT got message '%.*s' for topic '%s'\n", message->payloadlen, (char*)message->payload, message->topic);

	/*
	int ret = mosquitto_topic_matches_sub("/set/blah/+", message->topic, &match);
	if (ret != MOSQ_ERR_SUCCESS)
		syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: MQTT publish: %s", mosquitto_strerror(ret));
	if (match) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: MQTT got message for ADC topic\n");
	}
	*/
}
