#pragma once

#include <stdlib.h>
#include <syslog.h>
#include <stdint.h>
#include <stdbool.h>

#include "cmdline.h"

bool a2m_mqtt_setup(struct gengetopt_args_info args_info);
void a2m_mqtt_close();
int a2m_mqtt_publish(char* msg);
int a2m_mqtt_publish_connect(char* msg);
