#ifdef GL_ES
precision mediump float;
#endif

#define PI 3.141592653589
#define N 10

uniform float time;
uniform vec2 mouse;
uniform vec2 resolution;

vec4 rgba(int col) {
	float r = float(col / 0x10000) / 255.;
	float g = mod(float(col / 0x100), 256.) / 255.;
	float b = mod(float(col), 256.) / 255.;
	return vec4(r, g, b, 0.0);
}

void main(void) {
	float zoom = 100. + 0. * sin(time / 5.);
	vec2 pos = (gl_FragCoord.xy * 2. - resolution.xy) / min(resolution.x, resolution.y) * zoom;
	pos = vec2(cos(time*.1) * pos.x + sin(time*.1) * pos.y, cos(time*.1) * pos.y - sin(time*.1) * pos.x);
	//pos += vec2(time) * 2.
	
	//pos -= mod(pos, min(resolution.x, resolution.y) / 300. * sqrt(mouse.x));
	
	float z = 0.;
	for (int i = 1; i <= N; i++) {
		float a = PI * float(i) / float(N);
		z += cos(pos.x * cos(a) - pos.y * sin(a) + time * 4.);
	}
	
	gl_FragColor = mix(rgba(0xFF0000),
			   rgba(0x00FF00),
			   atan(z * 2.5) / PI + .5);
	// gl_FragColor *= 1.0 - exp(-pow(z * 2., 8.0));
	// if (2. * z > float(N)) gl_FragColor += vec4(0.6);
	// else gl_FragColor += 0.6 * exp(-pow(2. * z - float(N), 8.0));
}