uniform mat4 unif_matrix;
uniform mat4 unif_matrix_1;
uniform sampler2D cam0_texture;
uniform sampler2D logo_texture;
uniform float color_offset;
uniform float color_factor;
uniform float overlap;
uniform float cam0_aov;

const float M_PI = 3.1415926535;

varying float r0;
varying float r1;
varying float u0;
varying float v0;
varying float u1;
varying float v1;

void main(void) {
	if (r0 < cam0_aov / 360.0 - overlap) {
		if (u0 <= 0.0 || u0 > 1.0 || v0 <= 0.0 || v0 > 1.0) {
			gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
		} else {
			vec4 fc = texture2D(cam0_texture, vec2(u0, v0));
			fc = (fc - color_offset) * color_factor;

			gl_FragColor = fc;
		}
	} else {
		gl_FragColor = texture2D(logo_texture, vec2(u1, v1));
	}
}

