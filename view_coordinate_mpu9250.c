#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include "MotionSensor.h"

static float lg_compass_min[3] = { };
static float lg_compass_max[3] = { };
static float lg_compass[3];
static float lg_quat[4];

void *threadFunc(void *data) {

	do {
		ms_update();

		usleep(5000);
	} while (1);
}

static bool is_init = false;

void init_mpu9250() {
	if (is_init) {
		return;
	} else {
		is_init = true;
	}

	ms_open();

	//do{
	//	ms_update();
	//	printf("%f,%f,%f,%f\n",  (float)_q[0] / (1<<30),  (float)_q[1] / (1<<30),  (float)_q[2] / (1<<30),  (float)_q[3] / (1<<30));
	//	usleep(5000);
	//}while(1);
	pthread_t f1_thread;
	pthread_create(&f1_thread, NULL, threadFunc, NULL);
}

float *get_quatanion_mpu9250() {
	//convert from mpu coodinate to opengl coodinate
	lg_quat[0] = quatanion[1];	//x
	lg_quat[1] = quatanion[3];	//y : swap y and z
	lg_quat[2] = -quatanion[2];	//z : swap y and z
	lg_quat[3] = quatanion[0];	//w
	return lg_quat;
}

float *get_compass_mpu9250() {
	float bias[3];
	float gain[3];
	for (int i = 0; i < 3; i++) {
		lg_compass_min[i] = MIN(lg_compass_min[i]);
		lg_compass_max[i] = MAX(lg_compass_max[i]);
		bias = (lg_compass_min[i] + lg_compass_max[i]) / 2;
		gain = (lg_compass_max[i] - lg_compass_min[i]) / 2;
		lg_compass[i] = (compass[i] + bias[i]) / (gain[i] == 0 ? 1 : gain[i]);
	}
	return lg_compass;
}

float get_temperature_c_mpu9250() {
	return (temp - 32) * 5 / 9;
}
