#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 resolution;
uniform float time;

float niggaplease(float x, float y) {
	 return sin(1.0/2.0 * (x + y) * (x + y + 1.0) + y);
}

void main()
{
	vec2 p=(18.0*gl_FragCoord.xy-resolution)/max(resolution.x,resolution.y);
	for(int i=1;i<50;i++)
	{
		vec2 newp=p;
		newp.x+=0.6/float(i)*sin(float(i)*p.y+time*10.0/40.0+0.3*float(i))+1.0;
		newp.y+=0.6/float(i)*sin(float(i)*p.x+time*10.0/40.0+0.3*float(i+10))-1.4;
		p=newp;
	}
	vec3 col=vec3(0.5*sin(3.0*p.x)+niggaplease(p.x,p.y),
		      0.5*sin(3.0*p.y)+niggaplease(p.x,p.y),
		      sin(p.x+p.y)+niggaplease(p.x,p.y));
	gl_FragColor=vec4(col, 1.0);
}



