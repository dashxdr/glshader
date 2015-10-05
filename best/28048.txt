
#ifdef GL_ES
precision mediump float;
#endif

uniform float time;
uniform vec2 mouse;
uniform vec2 resolution;

	
float sdBox( vec3 p, vec3 b )
{
  vec3 d = abs(p) - b;
  return min(max(d.x,max(d.y,d.z)),0.0) + length(max(d,0.0));
}

float sdBox( vec2 p, vec2 b )
{
  vec2 d = abs(p) - b;
  return min(max(d.x,d.y),0.0) + length(max(d,0.0));
}



vec3 hash( vec3 p )
{
	p = vec3( dot(p,vec3(127.1,311.7,311.7)), dot(p,vec3(269.5,183.3,183.3)), dot(p,vec3(269.5,183.3,183.3)) );
	return fract(sin(p)*43758.5453);
}

float voronoi( vec3 x )
{
	vec3 n = floor(x);
	vec3 f = fract(x);
	vec3 mg, mr;

	float md = 8.0;
	{
		for( int j=-1; j<=1; j++ ) {
		for( int i=-1; i<=1; i++ ) {
		for( int k=-1; k<=1; k++ ) {
			vec3 g = vec3(float(i),float(j),float(k));
			vec3 o = hash( n + g );
			vec3 r = g + o - f;
			float d = dot(r,r);
			if( d<md ) {
				md = d;
				mr = r;
				mg = g;
			}
		}}}
	}

	md = 8.0;
	{
		for( int j=-1; j<=1; j++ ) {
		for( int i=-1; i<=1; i++ ) {
		for( int k=-1; k<=1; k++ ) {
			vec3 g = mg + vec3(float(i),float(j),float(k));
			vec3 o = hash( n + g );
			vec3 r = g + o - f;
			if( dot(mr-r,mr-r)>0.000001 ) {
				float d = dot( 1.5*(mr+r), normalize(r-mr) );
				md = min( md, d );
			}
		}}}
	}

	return md;
}


vec3 nrand3( vec2 co )
{
	vec3 a = fract( cos( co.x*8.3e-3 + co.y )*vec3(1.3e5, 4.7e5, 2.9e5) );
	vec3 b = fract( sin( co.x*0.3e-3 + co.y )*vec3(8.1e5, 1.0e5, 0.1e5) );
	vec3 c = mix(a, b, 0.5);
	return c;
}

float map(vec3 p)
{
    float h = 1.4;
    float rh = 0.25;
    float grid = 0.4;
    float grid_half = grid*0.5;
    float cube = 0.175;
    vec3 orig = p;

    vec3 g1 = vec3(ceil((orig.x)/grid), ceil((orig.y)/grid), ceil((orig.z)/grid));
    vec3 rxz =  nrand3(g1.xz);
    vec3 ryz =  nrand3(g1.yz);

    p = -abs(p);
    vec3 di = ceil(p/4.8);

    vec2 gap = vec2(rxz.x*rh, ryz.y*rh);
    float d1 = p.y + h + gap.x;
    float d2 = p.x + h + gap.y;

    vec2 p1 = mod(p.xz, vec2(grid)) - vec2(grid_half);
    float c1 = sdBox(p1,vec2(cube));

	return max(c1,d1);
}



vec3 genNormal(vec3 p)
{
    const float d = 0.01;
    return normalize( vec3(
        map(p+vec3(  d,0.0,0.0))-map(p+vec3( -d,0.0,0.0)),
        map(p+vec3(0.0,  d,0.0))-map(p+vec3(0.0, -d,0.0)),
        map(p+vec3(0.0,0.0,  d))-map(p+vec3(0.0,0.0, -d)) ));
}

void main()
{
    vec2 pos = (gl_FragCoord.xy*2.0 - resolution.xy) / resolution.y;
    vec3 camPos = vec3(-0.5,0.0,3.0);
    vec3 camDir = normalize(vec3(0.3, 0.0, -1.0));
    camPos -=  vec3(0.0,0.0,time*1.5);
    vec3 camUp  = normalize(vec3(0.5, 1.0, 0.0));
    vec3 camSide = cross(camDir, camUp);
    float focus = 1.8;

    vec3 rayDir = normalize(camSide*pos.x + camUp*pos.y + camDir*focus);	    
    vec3 ray = camPos;
    int march = 0;
    float d = 0.0;

    float total_d = 0.0;
    const int MAX_MARCH = 64;
    const float MAX_DIST = 100.0;
    for(int mi=0; mi<MAX_MARCH; ++mi) {
        d = map(ray);
        march=mi;
        total_d += d;
        ray += rayDir * d;
        if(d<0.001) {break; }
        if(total_d>MAX_DIST) {
            total_d = MAX_DIST;
            march = MAX_MARCH-1;
            break;
        }
    }
	
    float glow = 0.0;
    {
        const float s = 0.0075;
        vec3 p = ray;
        vec3 n1 = genNormal(ray);
        vec3 n2 = genNormal(ray+vec3(s, 0.0, 0.0));
        vec3 n3 = genNormal(ray+vec3(0.0, s, 0.0));
        glow = (1.0-abs(dot(camDir, n1)))*0.5;
        if(dot(n1, n2)<0.8 || dot(n1, n3)<0.8) {
            glow += 0.6;
        }
    }
    {
	vec3 p = ray;
        float grid1 = max(0.0, max((mod((p.x+p.y+p.z*2.0)-time*3.0, 20.0)-13.0)*0.5, 0.0) );
	    glow += clamp((1.0-voronoi(p)-0.97)*1000.0, 0.0, 1.0) * grid1;
        //vec3 gp1 = abs(mod(p, vec3(0.48)));
        //if(gp1.x<0.46 && gp1.z<0.46) {
        //    grid1 = 0.0;
        //}
        //glow += grid1;
    }

    float fog = min(1.0, (1.0 / float(MAX_MARCH)) * float(march))*1.0;
    vec3  fog2 = 0.00 * vec3(1, 1, 1.5) * total_d;
    glow *= clamp(4.0-(4.0 / float(MAX_MARCH-1)) * float(march), 0.0, 1.0);
    float scanline = mod(gl_FragCoord.y, 4.0) < 2.0 ? 0.7 : 1.0;
    gl_FragColor = vec4(vec3(0.15+glow*0.75, 0.15+glow*0.75, 0.2+glow)*fog + fog2, 1.0) * scanline;
}
