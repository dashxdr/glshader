#ifdef GL_ES
precision mediump float;
#endif

uniform float time;
uniform vec2 mouse;
uniform vec2 resolution;

void main( void ) {

	vec3 col = vec3(0.0);
	for (float i=0.; i<5.; i++)
	{
		vec2 p = gl_FragCoord.xy/resolution.x*1.5 
			- .35*vec2( 1.+sin(i+time), 1.+cos(i+time) );
		
		float a = atan(p.y, p.x);
	
		col.x += length(p) +
			 sin( a*15.0 +sin(5.0*time) ) ;
	
		if (col.x < 0.) col.y = -col.x;

		col.x = pow (.5*col.x, sin(time) + 15.5);
	}
	gl_FragColor = vec4(col, 1.0);

}