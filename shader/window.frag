varying vec4 position;

uniform mat4 unif_matrix;
uniform sampler2D cam0_texture;
uniform sampler2D logo_texture;
uniform float pixel_size;
//options start
uniform float sharpness_gain;
uniform float cam0_offset_yaw;
uniform float cam0_offset_x;
uniform float cam0_offset_y;
uniform float cam0_horizon_r;
//options end

const float M_PI = 3.1415926535;

void main(void) {
	float u = 0.0;
	float v = 0.0;
	vec4 pos = unif_matrix * position;
	float roll = asin(pos.y);
	float yaw = atan(pos.x, pos.z);
	float r = (M_PI / 2.0 - roll) / M_PI;
	//gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
	if (r < 0.6) {
		float yaw2 = yaw;
		u = cam0_horizon_r * r * cos(yaw2) + 0.5 + cam0_offset_x;
		v = cam0_horizon_r * r * sin(yaw2) + 0.5 - cam0_offset_y;
		vec4 fc;
		if (sharpness_gain == 0.0) {
			fc = texture2D(cam0_texture, vec2(u, v));
		} else {
			//sharpness
			fc = texture2D(cam0_texture, vec2(u, v))
					* (1.0 + 4.0 * sharpness_gain);
			fc -= texture2D(cam0_texture, vec2(u - 1.0 * pixel_size, v))
					* sharpness_gain;
			fc -= texture2D(cam0_texture, vec2(u, v - 1.0 * pixel_size))
					* sharpness_gain;
			fc -= texture2D(cam0_texture, vec2(u, v + 1.0 * pixel_size))
					* sharpness_gain;
			fc -= texture2D(cam0_texture, vec2(u + 1.0 * pixel_size, v))
					* sharpness_gain;
		}
		gl_FragColor = fc;
	} else {
		float yaw2 = -yaw;
		r = 1.0 - r;
		u = cam0_horizon_r * r * cos(yaw2) + 0.5;
		v = cam0_horizon_r * r * sin(yaw2) + 0.5;
//	    } else {
//	    	u = pos.x / pos.y * 0.2;
//	    	v = pos.z / pos.y * 0.2;
//	    	u = (-u + 1.0) / 2.0;
//	    	v = (-v + 1.0) / 2.0;
		gl_FragColor = texture2D(logo_texture, vec2(u, v));
	}
}