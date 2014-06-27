#ifdef GL_ES
precision mediump float;
#endif

uniform float time;
uniform vec2 mouse;
uniform vec2 resolution;

// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
// https://www.shadertoy.com/view/MdBGDK
// By David Hoskins.

float gTime = time;
#define TAU 3.14159265359*2.0

void main( void )
{
	float f = 3., g = 3.;
	vec2 mou;
	mou.x = sin(gTime*.3)*sin(gTime*.17)*1.+sin(gTime*.3);
	mou.y = (1.0-cos(gTime*.632))*sin(gTime*.131)*1.0+cos(gTime*.3);
	mou = (mou+1.0)*resolution.xy;
	vec2 z = ((-resolution.xy+2.0*gl_FragCoord.xy)/resolution.y);
	vec2 p = ((-resolution.xy+2.0+mou)/resolution.y);
	for( int i = 0; i < 20; i++) 
	{
		float d = dot(z,z);
		z = (vec2( z.x, -z.y ) / d)+p; 
		z.x =  abs(z.x);
		f = max( f, (dot(z-p,z-p) ));
		g = min( g, sin(mod(dot(z+p,z+p), TAU))+1.0);
	}
	f = abs(-log(f) / 3.5);
	g = abs(-log(g) / 8.0);
	gl_FragColor = vec4(min(vec3(g, g*f, f), 1.0),1.0);
}
