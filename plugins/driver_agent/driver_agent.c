#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>

#include "driver_agent.h"
#include "kokuyoseki.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define PLUGIN_NAME "driver_agent"

static PLUGIN_HOST_T *lg_plugin_host = NULL;

static void release(void *user_data) {
	free(user_data);
}

static int picam360_driver_xmp(char *buff, int buff_len, float light0_value,
		float light1_value, float motor0_value, float motor1_value,
		float motor2_value, float motor3_value) {
	int xmp_len = 0;

	xmp_len = 0;
	buff[xmp_len++] = 0xFF;
	buff[xmp_len++] = 0xE1;
	buff[xmp_len++] = 0; // size MSB
	buff[xmp_len++] = 0; // size LSB
	xmp_len += sprintf(buff + xmp_len, "http://ns.adobe.com/xap/1.0/");
	buff[xmp_len++] = '\0';
	xmp_len += sprintf(buff + xmp_len, "<?xpacket begin=\"﻿");
	buff[xmp_len++] = 0xEF;
	buff[xmp_len++] = 0xBB;
	buff[xmp_len++] = 0xBF;
	xmp_len += sprintf(buff + xmp_len, "\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>");
	xmp_len +=
			sprintf(buff + xmp_len,
					"<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"picam360-capture rev1\">");
	xmp_len +=
			sprintf(buff + xmp_len,
					"<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">");
	xmp_len += sprintf(buff + xmp_len, "<rdf:Description rdf:about=\"\">");
	xmp_len +=
			sprintf(buff + xmp_len,
					"<picam360_driver"
							" light0_value=\"%f\" light1_value=\"%f\""
							" motor0_value=\"%f\" motor1_value=\"%f\" motor2_value=\"%f\" motor3_value=\"%f\""
							" />", light0_value, light1_value, motor0_value,
					motor1_value, motor2_value, motor3_value);
	xmp_len += sprintf(buff + xmp_len, "</rdf:Description>");
	xmp_len += sprintf(buff + xmp_len, "</rdf:RDF>");
	xmp_len += sprintf(buff + xmp_len, "</x:xmpmeta>");
	xmp_len += sprintf(buff + xmp_len, "<?xpacket end=\"w\"?>");
	buff[xmp_len++] = '\0';
	buff[2] = ((xmp_len - 2) >> 8) & 0xFF; // size MSB
	buff[3] = (xmp_len - 2) & 0xFF; // size LSB

	return xmp_len;
}

#define LIGHT_NUM 2
#define MOTOR_NUM 4
static int lg_light_value[LIGHT_NUM] = { 0, 0 };
static int lg_motor_value[MOTOR_NUM] = { 0, 0, 0, 0 };
static float lg_light_strength = 0; //0 to 100
static float lg_thrust = 0; //-100 to 100
static float lg_brake_ps = 5; // percent
static bool lg_lowlevel_control = false;
static float lg_camera_quatanion[4] = { 0, 0, 0, 1 };
static float lg_target_quatanion[4] = { 0, 0, 0, 1 };

//kokuyoseki
static struct timeval lg_last_kokuyoseki_time = { };
static int lg_last_button = -1;
static int lg_func = -1;

static void *recieve_thread_func(void* arg) {
	int buff_size = 4096;
	unsigned char *buff = malloc(buff_size);
	unsigned char *buff_trash = malloc(buff_size);
	int data_len = 0;
	int marker = 0;
	int camd_fd = -1;
	bool xmp = false;
	char *buff_xmp = NULL;
	int xmp_len = 0;
	int xmp_idx = 0;

	while (1) {
		bool reset = false;
		if (camd_fd < 0) {
			char buff[256];
			sprintf(buff, "status");
			camd_fd = open(buff, O_RDONLY);
			if (camd_fd == -1) {
				printf("failed to open %s\n", buff);
				exit(-1);
			}
			printf("%s ready\n", buff);
		}
		data_len = read(camd_fd, buff, buff_size);
		if (data_len == 0) {
			printf("camera input invalid\n");
			break;
		}
		if (reset) {
			marker = 0;
			continue;
		}
		for (int i = 0; i < data_len; i++) {
			if (xmp) {
				if (xmp_idx == 0) {
					xmp_len = ((unsigned char*) buff)[i] << 8;
				} else if (xmp_idx == 1) {
					xmp_len += ((unsigned char*) buff)[i];
					buff_xmp = malloc(xmp_len);
					buff_xmp[0] = (xmp_len >> 8) & 0xFF;
					buff_xmp[1] = (xmp_len) & 0xFF;
				} else {
					buff_xmp[xmp_idx] = buff[i];
				}
				xmp_idx++;
				if (xmp_idx >= xmp_len) {
					char *xml = buff_xmp + strlen(buff_xmp) + 1;

					char *q_str = strstr(xml, "<quaternion");
					if (q_str) {
						float quatanion[4];
						sscanf(q_str,
								"<quaternion w=\"%f\" x=\"%f\" y=\"%f\" z=\"%f\" />",
								&quatanion[0], &quatanion[1], &quatanion[2],
								&quatanion[3]);
						//convert from mpu coodinate to opengl coodinate
						lg_camera_quatanion[0] = quatanion[1]; //x
						lg_camera_quatanion[1] = quatanion[3]; //y : swap y and z
						lg_camera_quatanion[2] = -quatanion[2]; //z : swap y and z
						lg_camera_quatanion[3] = quatanion[0]; //w

						lg_plugin_host->set_camera_quatanion(
								lg_camera_quatanion);
					}

					xmp = false;
					free(buff_xmp);
					buff_xmp = NULL;
				}
			}
			if (marker) {
				marker = 0;
				if (buff[i] == 0xE1) { //APP1
					xmp = true;
					xmp_len = 0;
					xmp_idx = 0;
				}
			} else if (buff[i] == 0xFF) {
				marker = 1;
			}
		}
	}

	return NULL;
}

void *transmit_thread_func(void* arg) {
	int xmp_len = 0;
	int buff_size = 4096;
	char buff[buff_size];
	static struct timeval last_time = { };
	gettimeofday(&last_time, NULL);
	while (1) {
		static struct timeval time = { };
		gettimeofday(&time, NULL);
		struct timeval diff;
		timersub(&time, &last_time, &diff);
		float diff_sec = (float) diff.tv_sec + (float) diff.tv_usec / 1000000;
		//cal
		if (!lg_lowlevel_control) {
			//trancate min max
			lg_light_strength = MIN(MAX(lg_light_strength, 0), 100);
			lg_light_value[0] = lg_light_strength;
			lg_light_value[1] = lg_light_strength;

			//trancate min max
			lg_thrust = MIN(MAX(lg_thrust, -100), 100);
			//brake
			lg_thrust *= exp(log(1.0 - lg_brake_ps / 100) * diff_sec);

			//(RtRc-1Rt-1)*Rt*vtg, target coordinate will be converted into camera coordinate
			float vtg[16] = { 0, -1, 0, 1 }; // looking at ground
			float unif_matrix[16];
			float camera_matrix[16];
			float target_matrix[16];
			mat4_identity(unif_matrix);
			mat4_identity(camera_matrix);
			mat4_identity(target_matrix);
			mat4_fromQuat(camera_matrix, lg_camera_quatanion);
			mat4_fromQuat(target_matrix, lg_target_quatanion);
			mat4_invert(camera_matrix, camera_matrix);
			mat4_multiply(unif_matrix, unif_matrix, camera_matrix); // Rc-1
			mat4_multiply(unif_matrix, unif_matrix, target_matrix); // RtRc-1

			mat4_transpose(vtg, vtg);
			mat4_multiply(vtg, vtg, unif_matrix);
			mat4_transpose(vtg, vtg);

			float xz = sqrt(vtg[0] * vtg[0] + vtg[2] * vtg[2]);
			float yaw = atan2(vtg[2], vtg[0]);
			float pitch = atan2(xz, -vtg[1]);
			printf("yaw=%f,\tpitch=%f\n", yaw, pitch);

			//lg_motor_value[0] = lg_thrust;
			//lg_motor_value[1] = -lg_thrust;
			//lg_motor_value[2] = -lg_thrust;
			//lg_motor_value[3] = lg_thrust;
		}
		//kokuyoseki func
		if (lg_last_button == BLACKOUT_BUTTON && lg_func != -1) {
			timersub(&time, &lg_last_kokuyoseki_time, &diff);
			diff_sec = (float) diff.tv_sec + (float) diff.tv_usec / 1000000;
			if (diff_sec > 0.5) {
				printf("func %d: ", lg_func);
				switch (lg_func) {
				case 1:
					lg_light_strength = 0;
					printf("light off\n");
					break;
				case 2:
					lg_thrust = 0;
					printf("thrust off\n");
					break;
				}
				if (lg_func > 10) {
					exit(0);
				}
				lg_func = -1;
			}
		}

		xmp_len = picam360_driver_xmp(buff, sizeof(buff), lg_light_value[0],
				lg_light_value[1], lg_motor_value[0], lg_motor_value[1],
				lg_motor_value[2], lg_motor_value[3]);
		int fd = open("driver", O_RDWR);
		if (fd > 0) {
			write(fd, buff, xmp_len);
			close(fd);
		}

		last_time = time;
		usleep(100 * 1000); //less than 10Hz
	}
}

static void command_handler(void *user_data, char *_buff) {
	char buff[256];
	strncpy(buff, _buff, sizeof(buff));
	char *cmd;
	cmd = strtok(buff, " \n");
	if (cmd == NULL) {
		//do nothing
	} else if (strncmp(cmd, PLUGIN_NAME ".set_light_value", sizeof(buff))
			== 0) {
		char *param = strtok(NULL, " \n");
		if (param != NULL) {
			float value;
			sscanf(param, "%f", &value);

			lg_light_value[0] = value;
			lg_light_value[1] = value;
			printf("set_light_value : completed\n");
		}
	} else if (strncmp(cmd, PLUGIN_NAME ".set_motor_value", sizeof(buff))
			== 0) {
		char *param = strtok(NULL, " \n");
		if (param != NULL) {
			int id = 0;
			float value = 0;
			sscanf(param, "%d=%f", &id, &value);
			if (id < MOTOR_NUM) {
				lg_motor_value[id] = value;
			}
			printf("set_motor_value : completed\n");
		}
	} else {
		printf(":unknown command : %s\n", buff);
	}
}

static void kokuyoseki_callback(struct timeval time, int button, int value) {
	if (value == 1) {
		return;
	}
	struct timeval diff;
	timersub(&time, &lg_last_kokuyoseki_time, &diff);
	float diff_sec = (float) diff.tv_sec + (float) diff.tv_usec / 1000000;
	switch (button) {
	case NEXT_BUTTON:
		lg_thrust += 1;
		printf("thrust %f\n", lg_thrust);
		break;
	case BACK_BUTTON:
		lg_thrust -= 1;
		printf("thrust %f\n", lg_thrust);
		break;
	case NEXT_BUTTON_LONG:
		if (diff_sec < 0.25)
			return;
		lg_light_strength += 1;
		printf("light %f\n", lg_light_strength);
		break;
	case BACK_BUTTON_LONG:
		if (diff_sec < 0.25)
			return;
		lg_light_strength -= 1;
		printf("light %f\n", lg_light_strength);
		break;
	case BLACKOUT_BUTTON:
		lg_func++;
		break;
	}
	{
		float *quat;
		switch (button) {
		case NEXT_BUTTON:
		case BACK_BUTTON:
			quat = lg_plugin_host->get_view_quatanion();
			if (quat) {
				memcpy(lg_target_quatanion, quat, sizeof(lg_target_quatanion));
			}
		}
	}
	lg_last_kokuyoseki_time = time;
	lg_last_button = button;
}

static bool is_init = false;
static void init() {
	if (!is_init) {
		is_init = true;

		set_kokuyoseki_callback(kokuyoseki_callback);
		open_kokuyoseki();

		pthread_t transmit_thread;
		pthread_create(&transmit_thread, NULL, transmit_thread_func,
				(void*) NULL);

		pthread_t recieve_thread;
		pthread_create(&recieve_thread, NULL, recieve_thread_func,
				(void*) NULL);
	}
}

void create_driver_agent(PLUGIN_HOST_T *plugin_host, PLUGIN_T **_plugin) {
	init();
	lg_plugin_host = plugin_host;

	PLUGIN_T *plugin = (PLUGIN_T*) malloc(sizeof(PLUGIN_T));
	strcpy(plugin->name, PLUGIN_NAME);
	plugin->release = release;
	plugin->command_handler = command_handler;
	plugin->user_data = plugin;

	*_plugin = plugin;
}
