/*

	airsensor2mqtt.c
	by: Markus Reschka https://github.com/mreschka/airsensor2mqtt

	Idea from:
	Rodric Yates http://code.google.com/p/airsensor-linux-usb/
	Ap15e (MiOS) http://wiki.micasaverde.com/index.php/CO2_Sensor
	Sebastian Sjoholm, sebastian.sjoholm@gmail.com
	FHEM Module CO2 https://svn.fhem.de/trac/browser/trunk/fhem/FHEM/38_CO20.pm

*/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>

#include "cmdline.h"
#include "a2m_mqtt.h"
#include "a2m_usb.h"

void clean_exit(int ret);
void sig_handler(int signo);

void parse_cmdline_config(int argc, char** argv);
int load_config_file(const char* fname, struct cmdline_parser_params* params);
void save_config_file();

struct gengetopt_args_info args_info;

void clean_exit(int ret) {
	a2m_usb_close();
	a2m_mqtt_close();
	syslog(LOG_MAKEPRI(LOG_USER, LOG_NOTICE), "NOTICE: Exiting.");
	closelog();
	exit(ret);
}

void sig_handler(int signo)
{
	if (signo == SIGINT) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_NOTICE), "NOTICE: received SIGINT.");
		clean_exit(1);
	}
	else if (signo == SIGTERM) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_NOTICE), "NOTICE: received SIGTERM.");
		clean_exit(1);
	}

}

int main(int argc, char* argv[])
{
	char mqttBuffer[50];

	//parse config from file and cmdline
	parse_cmdline_config(argc, argv);

	//configure logging
	if (args_info.verbose_given || args_info.debug_given) {
		openlog("airsensor2mqtt", LOG_CONS | LOG_PID | LOG_NDELAY | LOG_PERROR, LOG_USER);
	} else {
		openlog("airsensor2mqtt", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
	}
	//chose log level
	setlogmask(LOG_UPTO(LOG_NOTICE));

	if (args_info.debug_given) {
		setlogmask(LOG_UPTO(LOG_DEBUG));
	}
	else if (args_info.verbose_given) {
		setlogmask(LOG_UPTO(LOG_INFO));
	}
	syslog(LOG_MAKEPRI(LOG_USER, LOG_NOTICE), "NOTICE: Program started by User %d", getuid());
	syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: Debug messages active");

	//check whether we shall export a config file
	save_config_file();

	//setup signal handler
	if (signal(SIGINT, sig_handler) == SIG_ERR)
		syslog(LOG_MAKEPRI(LOG_USER, LOG_WARNING), "WARNING: Can't catch SIGINT");
	if (signal(SIGTERM, sig_handler) == SIG_ERR)
		syslog(LOG_MAKEPRI(LOG_USER, LOG_WARNING), "WARNING: Can't catch SIGTERM");

	//init mqtt client
	syslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), "INFO: MQTT: connecting");
	if (!a2m_mqtt_setup(args_info)) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: MQTT: Start failed. Exiting.");
		clean_exit(1);
	};
	syslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), "INFO: MQTT: connected");

	if (!a2m_usb_init())
		clean_exit(1);

	if (!a2m_usb_open())
		clean_exit(1);

	char *msg2 = "2";
	a2m_mqtt_publish_connect(msg2);

	usleep(50000);

	a2m_usb_flush();
	usleep(250000);

	unsigned short voc;
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	while(0==0) {

		t = time(NULL);
		tm = *localtime(&t);

		voc = a2m_usb_read_voc();
		a2m_usb_flush();

 		// check voc is between 450 and 2000
		if ( voc >= 450 && voc <= 2000) {
			if (args_info.verbose_given) {
				printf("%04d-%02d-%02d %02d:%02d:%02d, ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
				printf("VOC: %d, RESULT: OK\n", voc);
			}
			syslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), "VOC: %d, RESULT: OK", voc);
			sprintf(mqttBuffer, "%d", voc);
			a2m_mqtt_publish(mqttBuffer);
		} else {
			if (args_info.verbose_given) {
				printf("%04d-%02d-%02d %02d:%02d:%02d, ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
				printf("VOC: %d, RESULT: Error value out of range\n", voc);
			}
			syslog(LOG_MAKEPRI(LOG_USER, LOG_INFO), "VOC: %d, RESULT: Error value out of range", voc);
			sprintf(mqttBuffer, "0");
			a2m_mqtt_publish(mqttBuffer);
		}

		// If one read, then exit
		if (args_info.one_read_given) {
			clean_exit(0);
		}

		// Wait for next request for data
		sleep(args_info.wait_time_arg);
	}
}

void save_config_file()
{
	int ret;
	//check if we shall save the config to a file
	if (args_info.save_config_given) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_DEBUG), "DEBUG: Storing cmdline args to file.");
		//remove load and save flag - we don't export those to a file
		args_info.save_config_given = 0;
		args_info.load_config_given = 0;

		//save config
		ret = cmdline_parser_file_save(args_info.save_config_arg, &args_info);

		//check result
		if (ret != 0) {
			syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: Error saving config file: %d.", ret);
			printf("ERROR Configfile could not be saved to %s\n", args_info.save_config_arg);
		}
		else {
			printf("Configfile saved to %s\nExit.\n", args_info.save_config_arg);
		}

		//don't start deamon
		clean_exit(ret);
	}
}

void parse_cmdline_config(int argc, char** argv)
{
	struct cmdline_parser_params* params;
	params = cmdline_parser_params_create();

	//Disable Checks, we will check when everything is finished
	params->check_required = 0;
	//Enable Override from beginning on, i.e. the last config found can override everything before
	params->override = 1;
	//initialize parser info and tell not to do that again
	cmdline_parser_init(&args_info);
	params->initialize = 0;

	//We don't know logfile confg, enable default...
	openlog("airsensor2mqtt", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
	setlogmask(LOG_NOTICE);

	/// call config file parsers for all config locations, the first working parser will enable override and disable init
	load_config_file("/usr/local/etc/airsensor2mqtt.conf", params);
	load_config_file("/usr/etc/airsensor2mqtt.conf", params);
	load_config_file("/etc/airsensor2mqtt.conf", params);
	load_config_file("~/etc/.airsensor2mqtt.conf", params);
	load_config_file("~/.airsensor2mqtt.conf", params);
	load_config_file(".airsensor2mqtt.conf", params);

	//now read cmdline otions
	if (cmdline_parser_ext(argc, argv, &args_info, params) != 0) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: Config from cmdline invalid");
		clean_exit(1);
	}

	//check if user wants to add another config file
	if (args_info.load_config_given) {
		char* config_file = malloc(strlen(args_info.load_config_arg) + 1);
		strcpy(config_file, args_info.load_config_arg);
		int ret;

		//only user file will count, forget old data
		cmdline_parser_init(&args_info);
		//load user file
		ret = load_config_file(config_file, params);
		free(config_file);

		if (ret == 0) {
			//add cmdline options again to override user file
			cmdline_parser_ext(argc, argv, &args_info, params); //ignore ret, it worked already above
		} else {
			syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: Config from configfile invalid");
			clean_exit(1);
		}
	}

	//check config plausiblity
	if (cmdline_parser_required(&args_info, argv[0]) != 0) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_ERR), "ERROR: Config invalid, check STDERR output to find hints.");
		clean_exit(1);
	}

	free(params);
	closelog();
}

int load_config_file(const char* fname, struct cmdline_parser_params* params)
{
	int ret = -1;
	syslog(LOG_MAKEPRI(LOG_USER, LOG_NOTICE), "NOTICE: Trying to load config file %s", fname);
	if (access(fname, R_OK) != -1) {
		syslog(LOG_MAKEPRI(LOG_USER, LOG_NOTICE), "NOTICE: Loading config file %s", fname);
		ret = cmdline_parser_config_file(fname, &args_info, params);
		if (ret == 0) {
			syslog(LOG_MAKEPRI(LOG_USER, LOG_NOTICE), "NOTICE: configfile found and successfully parsed.");
			params->initialize = 0;
		}
		else {
			syslog(LOG_MAKEPRI(LOG_USER, LOG_NOTICE), "NOTICE: Config file %s has errors", fname);
		}
	}
	return ret;
}

