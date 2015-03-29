// just fiddling around with fractals it's a bit spiraling out of control --joltzor
#ifdef GL_ES
precision mediump float;
#endif

uniform float time;
uniform vec2 mouse;
uniform vec2 resolution;

#define MAX_ITER 64

vec2 fractal(in vec2 p, in vec2 i) {
	return vec2(dot(i, vec2(p.x, -p.y)), dot(p, i.yx));	
}

vec3 color(in float c) {
	return vec3(
		c*sin(c*0.33),
		c*sin(c*0.66),
		c*sin(c)
	);
}
void main( void ) {

	vec2 p = (( gl_FragCoord.xy / resolution ) - 0.5) * 2.0;
	p.x *= resolution.x/resolution.y;

	float angle = time * 0.2;
	mat2 rot = mat2(cos(angle), -sin(angle),
		  sin(angle), cos(angle));
	p *= rot;
	vec2 seed = vec2(sin(angle)*0.75, 0.0);

	vec2 i = p;
	
	
	float c = 0.0;
	for (int n = 0; n < MAX_ITER; n++) {
		vec2 r = i;
		i = fractal(i,r) + seed;
		i = fractal(i,r) + seed.yx;
		i = fractal(i,r) + r;
		
		c += length(r);
		if (length(i) > 10.0) {
			break;
		}
	}
	c /= float(MAX_ITER);
	gl_FragColor = vec4( vec3( color(pow(dot(c+c, 2.0), 2.2)) ), 1.0 );

}