#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <limits.h>
#include <dirent.h>
#include <dlfcn.h>
#include <assert.h>
#include <wordexp.h>

#include "view_point_multiplexor.h"
#include "mrevent.h"
#include "tools.h"

#define PLUGIN_NAME "view_point_multiplexor"
#define STREAMER_NAME "view_point_multiplexor"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static PLUGIN_HOST_T *lg_plugin_host = NULL;

static void release(void *obj) {

	free(obj);
}

static int _command_handler(int argc, char *argv[]) {
	int opt;
	int ret = 0;
	char *cmd = argv[0];
	if (cmd == NULL) {
		//do nothing
	} else if (strcmp(cmd, "generate") == 0) {
		int fov = 120;
		char *i_str = NULL; //input
		char *r_str = NULL; //rendering
		char *e_str = NULL; //encode
		char *o_str = NULL; //output file
		int split = 9;
		optind = 1; // reset getopt
		while ((opt = getopt(argc, argv, "i:r:e:o:n:")) != -1) {
			switch (opt) {
			case 'i':
				i_str = optarg;
				break;
			case 'r':
				r_str = optarg;
				break;
			case 'e':
				e_str = optarg;
				break;
			case 'o':
				o_str = optarg;
				break;
			case 'n':
				sscanf(optarg, "%d", &split);
				break;
			}
		}
		if (i_str == NULL || r_str == NULL || e_str == NULL || e_str == NULL
				|| o_str == NULL) {
			return -1;
		}
		for (int p = -split; p <= split; p++) {
			int pitch = 90 * p / split;
			int split_p = abs(p) == split ? 1 : 4 * (split - abs(p));
			for (int y = 0; y < split_p; y++) {
				int yaw = 360 * y / split_p;

				VECTOR4D_T vq = quaternion_init();
				vq = quaternion_multiply(vq, quaternion_get_from_z(0));
				vq = quaternion_multiply(vq, quaternion_get_from_x(pitch));
				vq = quaternion_multiply(vq, quaternion_get_from_y(yaw));

				char def[512];
				int len = 0;
				len += snprintf(def + len, sizeof(def) - len, "%s", i_str);
				len += snprintf(def + len, sizeof(def) - len,
						"!%s view_quat=%.3f,%.3f,%.3f,%.3f fov=%d", r_str, vq.x,
						vq.y, vq.z, vq.w, fov);
				len += snprintf(def + len, sizeof(def) - len, "!%s", e_str);
				len +=
						snprintf(def + len, sizeof(def) - len,
								"!image_recorder base_path=%s/%d_0_%d mode=RECORD",
								o_str, pitch, yaw);

				uuid_t uuid;
				uuid_generate(uuid);

				VSTREAMER_T *vstreamer = lg_plugin_host->build_vstream(uuid,
						def);
				vstreamer->start(vstreamer);
				printf("started : %s\n", def);

				while (1) {
					char eof_str[8];
					vstreamer->get_param(vstreamer, "eof", eof_str,
							sizeof(eof_str));
					if (eof_str[0] == '1' || eof_str[0] == 't'
							|| eof_str[0] == 'T') {
						printf("finished : %s\n", def);
						break;
					}
					usleep(100 * 1000);
				}
				lg_plugin_host->destroy_vstream(uuid);
			}
		}
		printf("%s : completed\n", cmd);
	}
}

static int command_handler(void *user_data, const char *_buff) {
	int opt;
	int ret = 0;
	char buff[512];
	strncpy(buff, _buff, sizeof(buff));

	wordexp_t p;
	ret = wordexp(buff, &p, 0);
	if (ret != 0) {
		printf("command parse error : %s", buff);
	} else {
		ret = _command_handler(p.we_wordc, p.we_wordv);
	}
	wordfree(&p);

	return 0;
}

static void event_handler(void *user_data, uint32_t node_id, uint32_t event_id) {
}

static void init_options(void *user_data, json_t *_options) {
	PLUGIN_T *plugin = (PLUGIN_T*) user_data;
	json_t *options = json_object_get(_options, PLUGIN_NAME);
	if (options == NULL) {
		return;
	}
}

static void save_options(void *user_data, json_t *_options) {
	json_t *options = json_object();
	json_object_set_new(_options, PLUGIN_NAME, options);

}

void create_plugin(PLUGIN_HOST_T *plugin_host, PLUGIN_T **_plugin) {
	lg_plugin_host = plugin_host;

	{
		PLUGIN_T *plugin = (PLUGIN_T*) malloc(sizeof(PLUGIN_T));
		memset(plugin, 0, sizeof(PLUGIN_T));
		strcpy(plugin->name, PLUGIN_NAME);
		plugin->release = release;
		plugin->command_handler = command_handler;
		plugin->event_handler = event_handler;
		plugin->init_options = init_options;
		plugin->save_options = save_options;
		plugin->get_info = NULL;
		plugin->user_data = plugin;

		*_plugin = plugin;
	}
}
