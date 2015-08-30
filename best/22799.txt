#ifdef GL_ES
precision mediump float;
#endif

uniform float time;
uniform vec2 mouse;
uniform vec2 resolution;
uniform sampler2D back;

//Smooth Life like thing

//#define VIEW_REGIONS
//#define VIEW_LIFE_FUNCTION

#define sqr(x) (x*x)

const int KERNEL_SIZE = 12;
const float OUTER_RADIUS = 12.0; //Radius of the neigbor region
const float INNER_RADIUS = 7.0; //Radius of the cell region

float timestep = 0.15;

//These values modify the "life function" messaround with them to get different patterns.

float eb = 0.001; //Edge blur

float uts = 0.49; //Upper threshold start
float ute = 0.65; //Upper threshold end

float lts = 0.06; //Lower threshold start
float lte = 0.6; //Lower threshold end

float bp = 0.20; //Blending position
float bw = 0.30; //Blending width

vec2 center = resolution.xy/2.0;

//Returns what region a sample is in (cell/neighbor region).
vec2 getRegion(vec2 offset)
{
	float distSq = dot(offset,offset);
	
	vec2 region = vec2
	(
		step(-sqr(INNER_RADIUS),-distSq), //Cell
		step(sqr(INNER_RADIUS),distSq) * step(-sqr(OUTER_RADIUS),-distSq) //Neighbors
	);
	
	return region;
}

//Returns the cell's state as a function of how much it's regions are filled.
float lifeFunction(vec2 regFill)
{
	float blend = smoothstep(bp-bw*0.5,bp+bw*0.5,regFill.y); //Threshold Blending
	
	float lt = mix(lts,uts,blend); //Low Threshold
	float ut = mix(lte,ute,blend); //High Threshold
	
	float state = smoothstep(lt,lt+eb,regFill.x) * smoothstep(ut,ut-eb,regFill.x); //Threshold Area
	
	return state;
}

//Sums up all of the cells in the cell/neighbor regions and returns them as a percentage of how full the regions are.
//x : The "health" of the cell.
//y : How crowded the surrounding area is.
vec2 kernel(vec2 uv)
{
	vec2 fill = vec2(0);
	vec2 maxFill = vec2(0);
	
	//Loop through every sample in the kernel's radius.
	for(int i = -KERNEL_SIZE;i <= KERNEL_SIZE;i++)
	{
		for(int j = -KERNEL_SIZE;j <= KERNEL_SIZE;j++)
		{
			vec2 offset = vec2(i,j);
			
			vec2 region = getRegion(offset);
			
			fill += region * texture2D(back,mod((uv+offset)/resolution,1.0)).a;
			
			maxFill += region;	
		}
	}
	
	return fill/maxFill;
}

void main( void ) 
{
	vec2 uv = gl_FragCoord.xy;

	vec3 color = vec3(0.0);
	
	vec2 regionFill = kernel(uv);
	
	float cur = texture2D(back,uv/resolution).a;
	float next = lifeFunction(regionFill);
	
	float state = cur+(next-cur)*timestep; //Step the simulation forward by a defined timestep.
	
	if(length(mouse) < 0.05 || time < 0.75)
	{
		state =	step(-64.0,-length(uv-center));
	}
	
	if(length(uv-(mouse*resolution)) < 8.0)
	{
		state = 1.0;
	}
	
	color = vec3(smoothstep(0.2,0.25,regionFill.x)-smoothstep(0.25,0.30,regionFill.y)+pow(regionFill.y,3.0));
	
	gl_FragColor = vec4(color, state);
	
	#ifdef VIEW_REGIONS
	gl_FragColor.rg = getRegion(uv-center);
	#endif
	#ifdef VIEW_LIFE_FUNCTION
	gl_FragColor.rg = vec2(lifeFunction(uv/resolution));
	#endif
}