#ifdef GL_ES
precision mediump float;
#endif

uniform float time;
uniform vec2 mouse;
uniform vec2 resolution;

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float grid(vec2 p)
{
	if (mod(p.x - 0.01, 1.0) > 0.98 || mod(p.y - 0.01, 1.0) > 0.98)
		return 1.0;
	else
		return 0.0;
}

float mandel(vec2 p)
{
	float re = 0.0, im = 0.0, nre = 0.0, nim = 0.0, sqd = 0.0;
	
	for (int iters = 0; iters < 256; iters++)
	{
		// z -> z^2 + c
		// re + im*i -> (re*im*i)^2 + c -> (re^2 - im^2 + c.x) + (2*re*im + c.y)i
		nre = re * re - im * im + p.x;
		nim = 2.0 * re * im + p.y;
		re = nre;
		im = nim;
		
		float nsqd = re * re + im * im;
		if (nsqd > 4.0)
		{
			return float(iters) - 4.0 / (1.5 + (nsqd - 4.0));
		}
		sqd = nsqd;
		
	}
	
	return 256.0;
}

vec2 getTarget(float phase)
{
	
	int which = int(mod(phase, 3.0));
	if (which == 0)
		return vec2(0.26992, 0.480736);
	if (which == 1)
		return vec2(-0.745626, 0.100045);
	else
		return vec2(-1.112111, 0.249920);
}

vec4 mc(float m)
{
	if (m >= 255.5) return vec4(0, 0, 0, 0);
	
	m += time * 16.0;
	float h = 0.2 + 0.6 * mod(floor(m/24.0) * 0.12 + 0.45, 1.0);
	float s = 0.4 + 0.6 * mod(m/24.0, 1.0);
	float v = 1.0 - 0.9 * mod(m/16.0, 1.0);
	
	return vec4(hsv2rgb(vec3(h, s, v)), 1.0);
}

mat2 rot(float t)
{
	float c = cos(t), s = sin(t);
	return mat2(c, s, -s, c);
}

void main( void ) 
{
	float ar = resolution.y / resolution.x;
	vec2 p = (gl_FragCoord.xy / resolution.xy - vec2(0.5, 0.5)) * vec2(3.0 / ar, 3.0);
	float phase = 0.5 - 0.5 * cos(time * 0.333333);
	float tph = time / (6.0 * 3.1415926535);
	vec2 target = getTarget(tph);
	float scale = 1.0 / pow(2.0, phase * 16.0);
	
	if (mod(tph, 1.0) > 0.90) {
		target *= 1.0 - pow(10.0 * (mod(tph, 1.0) - 0.90), 3.0);
	}
	
	if (mod(tph, 1.0) < 0.10) {
		target *= 1.0 - pow(10.0 * (0.1 - mod(tph, 1.0)), 3.0);
	}
	
	p *= scale;
	p = p * rot(phase * 8.0);
	p += target;
	
	float m = mandel(p);
	
	vec4 under = mc(m);
	
	//vec4 g = mix(vec4(1.0, 1.0, 0.0, 1.0), under, 1.0 - 0.2 * grid(p));
	
	gl_FragColor = under;
}