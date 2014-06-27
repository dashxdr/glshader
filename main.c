#include <fcntl.h>
#include <stdlib.h>

#include <stdarg.h>
#include <ctype.h>
#include "misc.h"

#define GL_GLEXT_PROTOTYPES 1
#include <SDL/SDL_opengl.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL.h>

//#define USE_FB

#define CAPTION "shadermovie"

char *moviename = "/tmp/shadermovie.mp4";

#define MAX_SHADERS 1024
int numshaders = 0;
int currentshader = 0;
char *shaders[MAX_SHADERS];
long long timing_usec=0;
int timing_framecount=0;
int timing_enabled = 0;

typedef int int32;
typedef unsigned int uint32;

#define SWAPTIME 20

struct pic {
unsigned char *image;
int w,h;
};


int readppm(char *name,struct pic *pic);
void saveppm(int count);


float centerx=0.0, centery=0.0, zoom=1.0;

int mousex,mousey,sizex,sizey;
float mx = 0.0, my = 0.0;
void setmxmy(int mousex, int mousey)
{
	float factor = sizex>sizey ? sizey : sizex;
	factor = 16.0 / factor;

	mx = factor * (mousex - sizex/2);
	my = factor * (sizey/2 - mousey);
}


struct timeval starttime;

void inittime()
{
	gettimeofday(&starttime,0);
	timing_usec = 0;
	timing_framecount = 0;
}
uint32 gtime()
{
struct timeval tv;
int s,m;

	gettimeofday(&tv,0);
	s=tv.tv_sec-starttime.tv_sec;
	m=tv.tv_usec-starttime.tv_usec;
	if(m<0) {m+=1000000;--s;}
	return s*1000+m/1000;
}
uint32 ugtime(void)
{
struct timeval tv;
int s,m;

	gettimeofday(&tv,0);
	s=tv.tv_sec-starttime.tv_sec;
	m=tv.tv_usec-starttime.tv_usec;
	return s*1000000+m;
}

float random0to1(void)
{
#define BIGNUM 10000000
	return (rand()%BIGNUM) * (1.0 / (BIGNUM-1));
}

void randomcolor(GLfloat *p)
{
	p[0] = random0to1();
	p[1] = random0to1();
	p[2] = random0to1();
	p[3] = 1.0;
}



uint32 torgba(unsigned int r,unsigned int g,unsigned int b,unsigned int a)
{
	r&=255;
	g&=255;
	b&=255;
	a&=255;
	return (a<<24) | (r<<0) | (g<<8) | (b<<16);
}




SDL_Surface *setvideomode(int w, int h)
{
	SDL_Surface *screen;
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	screen = SDL_SetVideoMode(w, h, 0, SDL_OPENGL | SDL_RESIZABLE);
	if(!screen)
	{
		fprintf(stderr, "Couldn't set %dx%d GL video mode: %s\n",
			sizex, sizey, SDL_GetError());
		SDL_Quit();
		exit(2);
	}
	return screen;
}

void initview(int width, int height)
{
	glViewport(0, 0, width, height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}
void init_off_view(void)
{
	initview(sizex, sizey);
}

#define PIECE_SCALE 15.0
static void resize( unsigned int width, unsigned int height )
{
	sizex=width;
	sizey=height;

	setvideomode(width, height);

	initview(sizex, sizey);
}







void setgrey(float v)
{
float col[4]={v,v,v,v};
	glMaterialfv(GL_FRONT,GL_AMBIENT_AND_DIFFUSE, col);

}


inline double dist(double dx, double dy)
{
	return sqrt(dx*dx + dy*dy);
}

int downx,downy;
int isdown,justdown,justup;
int pressed = -1;
int downbutton = -1;

void button_init(void)
{
	justdown = justup = 0;
}
void down(int button,int x,int y)
{
	if(button==5 || button==4)
	{
		zoom *= pow(1.1, (button==5) ? 1.0 : -1.0);
		return;
	}
	downbutton = button;
	downx=x;
	downy=y;
	isdown=1;
	justdown=1;
}
void up(int button,int x,int y)
{
	isdown=0;
	justup=1;
}

#define ABS(x) (((x)<0) ? (-(x)) : (x))
void moved(int x,int y)
{
	if(isdown)
	{
		int dx, dy;
		dx = x - downx;
		dy = y - downy;
		downx = x;
		downy = y;
		if(downbutton == 1)
		{
			int which = (sizex>sizey) ? sizey : sizex;
			float fix = 1.0 / (zoom * which);
			centerx -= dx*fix;
			centery += dy*fix;
		} else
		{
			zoom *= pow(1.002, dy);
		}
	}
}

#define MAXCODES 64
static int downcodes[MAXCODES];
static int pressedcodes[MAXCODES];
static int numcodes=0,numpressed=0;

void processkey(int key,int state)
{
int i;
	if(state)
	{
		if(numpressed<MAXCODES) pressedcodes[numpressed++]=key;
		for(i=0;i<numcodes;++i)
			if(downcodes[i]==key) return;
		if(numcodes<MAXCODES)
			downcodes[numcodes++]=key;
	} else
	{
		for(i=0;i<numcodes;)
		{
			if(downcodes[i]==key)
				downcodes[i]=downcodes[--numcodes];
			else
				++i;
		}

	}
}
int IsDown(int key)
{
int i;
	for(i=0;i<numcodes;++i)
		if(downcodes[i]==key) return 1;
	return 0;
}

int WasPressed(int key)
{
int i;
	for(i=0;i<numpressed;++i)
		if(pressedcodes[i]==key) return 1;
	return 0;
}

void typedkey(int key)
{
	if(key<0 || key>0x7f) return;
}

#ifdef USE_FB
struct target
{
	GLuint framebuffer;
	GLuint renderbuffer;
	GLuint texture;
} targets[2];
int ping = 0;

void createtarget(struct target *target, int width, int height)
{
	glGenFramebuffersEXT(1, &target->framebuffer);
	glGenRenderbuffersEXT(1, &target->renderbuffer);
	glGenTextures(1, &target->texture);
// set up framebuffer
	glBindTexture(GL_TEXTURE_2D, target->texture);
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, target->framebuffer);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
		 GL_TEXTURE_2D, target->texture, 0);
// set up renderbuffer
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, target->renderbuffer);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT16, width, height);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,
			GL_RENDERBUFFER_EXT, target->renderbuffer);

// clean up
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}
#endif


GLuint get_texture(int width, int height)
{
GLuint texture;
	// create a texture object
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);//GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);//GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height,
				 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	return texture;
}


GLuint bgtexture;

GLuint vertexshader, fragmentshader, mProgram=0;
GLuint ResolutionHandle, TimeHandle, MouseHandle, SizeHandle;
GLint attribute_coord3d, attribute_spos;
GLuint vertex_values;

const GLchar *vertexShaderCode = 
		"varying vec2 surfacePosition;\n"
		"attribute vec3 coord3d;\n"
		"attribute vec2 spos;\n"
		"void main(){\n"
//		" gl_Position = ftransform();\n"
//		" gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * gl_Vertex;\n"
		" gl_Position = vec4(coord3d, 1.0);\n"
		" surfacePosition = spos;\n"
		"}						 \n";

const GLchar *fragmentShaderCode = 
	"precision mediump float;  \n"
	"void main(){			  \n"
//	" gl_FragColor = vec4 (0.63671875, 0.76953125, 0.22265625, 1.0); \n"
	" gl_FragColor = vec4 (fract(gl_FragCoord.x*.01), fract(gl_FragCoord.y*.0), 0.9, 1.0); \n"
	"}						 \n";

void showerrors(int shader, char *name)
{
GLchar errorlog[8192];
GLsizei len;
	glGetShaderInfoLog(shader, sizeof(errorlog), &len, errorlog);
	if(len)
		printf("%s:\n%s\n", name, errorlog);
}
int loadshader(int type, const GLchar *shaderCode)
{
	
	// create a vertex shader type (GL_VERTEX_SHADER)
	// or a fragment shader type (GL_FRAGMENT_SHADER)
	int shader = glCreateShader(type); 
		
	// add the source code to the shader and compile it
	glShaderSource(shader, 1, &shaderCode, 0);
	glCompileShader(shader);
	showerrors(shader, "input");
		
	return shader;
}

int loadnamedshader(int type, char *name)
{
int fd;
GLchar *mem;
int size;
int shader = glCreateShader(type); 
int len;

	fd = open(name, O_RDONLY);
	if(fd<0) return shader;
	size = lseek(fd, 0l, SEEK_END);
	mem = calloc(size+1, 1);
	lseek(fd, 0l, SEEK_SET);
	len=read(fd, mem, size);
	len=len;
	mem[size] = 0;

	// add the source code to the shader and compile it
	glShaderSource(shader, 1, (const GLchar **)&mem, 0);
	glCompileShader(shader);
	showerrors(shader, name);

	free(mem);
	return shader;
}

GLfloat vertex_data[] = {
	0.0, 0.0, 0.0, 0.0, 0.0,
	0.0, 0.0, 0.0, 0.0, 0.0,
	0.0, 0.0, 0.0, 0.0, 0.0,
	0.0, 0.0, 0.0, 0.0, 0.0
};

void initshader(void)
{
	if(mProgram>0)
	{
		glDeleteProgram(mProgram);
		glDeleteShader(vertexshader);
		glDeleteShader(fragmentshader);
	}
	printf("shader:%s\n", shaders[currentshader]);

// create shader and program
	vertexshader = loadshader(GL_VERTEX_SHADER, vertexShaderCode);
//	fragmentshader = loadshader(GL_FRAGMENT_SHADER, fragmentShaderCode);
	fragmentshader = loadnamedshader(GL_FRAGMENT_SHADER,
			 shaders[currentshader]);

	mProgram = glCreateProgram();
	glAttachShader(mProgram, vertexshader);
	glAttachShader(mProgram, fragmentshader);
	glLinkProgram(mProgram);
	ResolutionHandle = glGetUniformLocation(mProgram, "resolution");
	TimeHandle = glGetUniformLocation(mProgram, "time");
	MouseHandle = glGetUniformLocation(mProgram, "mouse");
	SizeHandle = glGetUniformLocation(mProgram, "surfaceSize");
	attribute_coord3d = glGetAttribLocation(mProgram, "coord3d");
	attribute_spos = glGetAttribLocation(mProgram, "spos");
}

void clear_texture(void)
{
	unsigned char *rgb;
	int TWIDTH, THEIGHT;
	TWIDTH = THEIGHT = 4;
	rgb = calloc(TWIDTH * THEIGHT,4);
	glBindTexture(GL_TEXTURE_2D, bgtexture);
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, TWIDTH, THEIGHT, 0, GL_RGBA,
				GL_UNSIGNED_BYTE, rgb );
	free(rgb);
}


int oldnum = -1;
void newshader(int delta)
{
	static time_t oldstamp = 0;

	if(delta)
	{
		currentshader += delta;
		if(currentshader<0) currentshader += numshaders;
		if(currentshader >= numshaders) currentshader -= numshaders;
	}
	struct stat st;
	int res = stat(shaders[currentshader], &st);
	if(res) return;

	if(oldnum == currentshader)
	{
		if(st.st_mtime == oldstamp)
			return;
	} else
		oldnum = currentshader;
	oldstamp = st.st_mtime;

	initshader();
	clear_texture();
	zoom = 1.0;
	centerx = 0.0;
	centery = 0.0;
	inittime();
}

void deleteit(void)
{
	if(!numshaders) return;
	char *deldir = "deleted";
	mkdir(deldir, 0755);
	char temp[512];
	char *p = shaders[currentshader];
	char *s = p;
	p += strlen(p);
	while(p>s && p[-1] != '/') --p;
	snprintf(temp, sizeof(temp), "%s/%s", deldir, p);
	printf("moving %s to %s\n", shaders[currentshader], temp);
	int res = rename(shaders[currentshader], temp);
	if(res) printf("res = %d (%m)\n", res);
	memmove(shaders+currentshader, shaders+currentshader+1,
			(numshaders-currentshader-1)*sizeof(char *));
	--numshaders;
	oldnum = -1;
	newshader(currentshader==numshaders ? -1 : 0);
}

void initstuff(void)
{
	printf("GL_VERSION: %s\n", glGetString(GL_VERSION));

	bgtexture = get_texture(4, 4);
	clear_texture();

	glGenBuffers(1, &vertex_values);

	currentshader = 0;
	newshader(0);
#ifdef USE_FB
	createtarget(targets+0, sizex, sizey);
	createtarget(targets+1, sizex, sizey);
	ping = 0;
#endif
}

void copy_to_back(void)
{
	glEnable(GL_TEXTURE_2D);
		/* Target texture */
	glBindTexture(GL_TEXTURE_2D, bgtexture);
		/* Copy screen to a 1024x1024 texture */
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,0,0,
			sizex, sizey, 0);
}


static void draw(float time, int show);

#define INTERVAL 5

inline double min(double v1, double v2)
{
	return v1<v2 ? v1 : v2;
}

volatile int globaltime=0;
int timechase = 0;
#define INTERVAL_MSEC 20
Uint32 mytimer(Uint32 interval, void *param)
{
	++globaltime;
	return INTERVAL_MSEC;
}


static void event_loop( void )
{
char exitflag=0;
int t;
int nexttime;
int nframes=0;
SDL_Event event;
int code,mod;
int savecount = 0;
	SDL_AddTimer(INTERVAL_MSEC, mytimer, 0);

	inittime();
	nexttime=INTERVAL*1000;
	while(!exitflag)
	{
		while(timechase == globaltime)
			SDL_Delay(1);
		++timechase;

		t=gtime();
		if(0 && t>=nexttime)
		{
			printf("%3d frames in %d seconds:%6.2f frames/second\n",
				nframes,INTERVAL,(float)nframes/INTERVAL);
			nexttime+=INTERVAL*1000;
			nframes=0;
		}

		newshader(0);
		t = 1;
		while(t-- > 0)
			draw(gtime()*.001, (t==0));

		++nframes;

		numpressed=0;

		button_init();
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
			case SDL_MOUSEMOTION:
				mousex=event.motion.x;
				mousey=event.motion.y;
				setmxmy(mousex, mousey);
				moved(mousex, mousey);
				break;
			case SDL_MOUSEBUTTONDOWN:
				mousex=event.button.x;
				mousey=event.button.y;
				setmxmy(mousex, mousey);
				down(event.button.button,mousex, mousey);
				break;
			case SDL_MOUSEBUTTONUP:
				mousex=event.button.x;
				mousey=event.button.y;
				setmxmy(mousex, mousey);
				up(event.button.button,mousex, mousey);
				pressed = -1;
				break;
			case SDL_KEYDOWN:
				code=event.key.keysym.sym;
				mod=event.key.keysym.mod;
				mod=mod;
				if(code==SDLK_ESCAPE)
					exitflag=1;
				if(code=='g') saveppm(savecount++);
				if(code==SDLK_RIGHT) newshader(1);
				if(code==SDLK_LEFT) newshader(-1);
				if(code=='t')
				{
					timing_enabled = !timing_enabled;
					if(timing_enabled)
						inittime();
				}
				if(code=='x') deleteit();
				processkey(code, 1);
				break;
			case SDL_KEYUP:
				code=event.key.keysym.sym;
				processkey(code, 0);
				break;
			case SDL_VIDEORESIZE:
				resize(event.resize.w, event.resize.h);
				break;
			case SDL_VIDEOEXPOSE:
				SDL_GL_SwapBuffers();
				break;
			case SDL_QUIT:
				exitflag=1;
				break;				
// handle resize
			}
		}
	}
}

int ppmh;
int ppmin;
unsigned char ppmbuff[2048],*ppmp;
int ppmci()
{
	if(ppmin==0)
	{
		ppmin=read(ppmh,ppmbuff,sizeof(ppmbuff));
		ppmp=ppmbuff;
	}
	if(ppmin<=0) return -1;
	--ppmin;
	return *ppmp++;
}

void ppmline(unsigned char *put)
{
int c;
	while((c=ppmci())>=0)
		if(c==0x0a) break;
		else *put++=c;
	*put=0;
}

int readppm(char *name,struct pic *pic)
{
unsigned char line[8192];
int w,h;
unsigned int i,j;
unsigned char *put;

	ppmin=0;
	ppmh=open(name,O_RDONLY);
	if(ppmh<0) return 0;
	ppmline(line);
	if(strcmp((char *)line,"P6")) {close(ppmh);return 0;}
	ppmline(line);
	if(sscanf((char *)line,"%d %d",&w,&h)!=2) {close(ppmh);return 0;}
	pic->w=w;
	pic->h=h;
	pic->image=put=malloc(w*h*3);
	if(!put) {close(ppmh);return 0;}
	ppmline(line);
	for(j=0;j<h;++j)
	{
		for(i=0;i<w;++i)
		{
			*put++=ppmci();
			*put++=ppmci();
			*put++=ppmci();
		}
	}
	return 1;
}

void saveppm(int count)
{
unsigned char temp[16384];
char fn[64];
int f;
int y;
int res;
	sprintf(fn, "/tmp/save%03d.ppm", count);

	f = open(fn, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	sprintf((char *)temp, "P6\n%d %d\n255\n", sizex, sizey);
	res = write(f, (char *)temp, strlen((char *)temp));
	res=res;

	for(y=0;y<sizey;++y)
	{

		glReadPixels(0, sizey-1-y, sizex, 1, GL_COLOR_INDEX | GL_RGB,
			GL_UNSIGNED_BYTE, temp);

		res = write(f, temp, 3*sizex);
	}
	close(f);
}

#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 960

void sortshaders(void)
{
	char *findname(char *p)
	{
		char *t = p;
		while(*p)
		{
			if(*p++ == '/')
				t = p;
		}
		return t;
	}
	int comp(const void *_p1, const void *_p2)
	{
		char *p1 = findname(*(char **)_p1);
		char *p2 = findname(*(char **)_p2);
		int d1 = isdigit(*p1);
		int d2 = isdigit(*p2);
		if(d1 && d2)
			return atoi(p2) - atoi(p1);
		if(d1 && !d2) return -1;
		if(d2 && !d1) return 1;
		return strcmp(p2, p1);
	}
	qsort(shaders, numshaders, sizeof(shaders[0]), comp);
//	int i;for(i=0;i<numshaders;++i) printf("%4d: %s\n", i, shaders[i]);
}

void helptext(char *name)
{
	printf("Use: %s [options] shader ...\n", name);
	printf("   Options:\n");
	printf("   -g <width>x<height>     # example: -g %dx%d (the default)\n",
				DEFAULT_WIDTH, DEFAULT_HEIGHT);
	printf("   -o moviename.mp4        # default: /tmp/shadermovie.mp4\n");
	exit(0);
}

int main( int argc, char *argv[] )
{
	int tx, ty;
	int i;

//	if(fork()) return;

	srandom(gtime());

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	sizex = DEFAULT_WIDTH;
	sizey = DEFAULT_HEIGHT;

	numshaders = 0;
	for(i=1;i<argc;)
	{
		if(argv[i][0]=='-')
		{
			switch(argv[i++][1])
			{
			case 'g':
				if(i>=argc) helptext(argv[0]);
				if(sscanf(argv[i], "%dx%d", &tx, &ty)==2 && tx>0 && ty>0)
				{
					sizex = tx;
					sizey = ty;
					++i;
					continue;
				}
				helptext(argv[0]);
				break;
			case 'o':
				if(i>=argc) helptext(argv[0]);
				moviename = strdup(argv[i++]);
				continue;
			default:
				helptext(argv[0]);
				break;
			}
		}
		if(numshaders < MAX_SHADERS)			
			shaders[numshaders++] = strdup(argv[i]);
		else
			printf("Too many shaders, discarding %s\n", argv[i]);
		++i;
	}
	if(!numshaders)
		helptext(argv[0]);

	sortshaders();

	setvideomode(sizex, sizey);
	SDL_WM_SetCaption(CAPTION, CAPTION);

	resize(sizex, sizey);

	if(0)
	{
		char *p = strdup((const char *)glGetString(GL_EXTENSIONS));
		char *t;
		t = p;
		while(*t)
			if(*t==' ') *t++='\n';
			else ++t;
		printf("GL_EXTENSIONS:\n%s", p);
	}
//	glLightfv(GL_LIGHT0, GL_POSITION, lightpos);

	glShadeModel( GL_SMOOTH );

	glBlendFunc(GL_ONE, GL_ONE);

	initstuff();

	if(1)
	{
		event_loop();
	} else
	{
		encode_my_video("test.mp4", sizex, sizey, 1000, 24.0);
	}
	SDL_Quit();
	return 0;
}

void draw_frame(struct myframe *f)
{

	draw(f->pts, 1);
	unsigned char *temp = malloc(65536);
	if(!temp) return;

	int w, h, w3;
	w = f->width;
	h = f->height;
	w3 = w*3;

	int x, y;
	for(y=0;y<h;y+=2)
	{
		glReadPixels(0, h-2-y, sizex, 2, GL_COLOR_INDEX | GL_RGB,
			GL_UNSIGNED_BYTE, temp);
		unsigned char *yp = f->data[0] + y*w;
		int i;
		for(i=0;i<2;++i)
		{
			unsigned char *basergb = temp + (1-i)*w3;
			unsigned char *basey = yp + i*w;
			for(x=0;x<w;++x)
			{
				unsigned char *t = basergb + x*3;
				int yval = (19595*t[0] + 38470*t[1] + 7471*t[2]) >> 16;
				basey[x] = yval;
			}
		}
		int off = (y>>1) * (w>>1);
		unsigned char *up = f->data[1] + off;
		unsigned char *vp = f->data[2] + off;

		for(x=0;x<w;x+=2)
		{
			unsigned char *t = temp + x*3;
			int r, g, b;
			r = (t[0] + t[3] + t[w3+0] + t[w3+3])>>2;
			g = (t[1] + t[4] + t[w3+1] + t[w3+4])>>2;
			b = (t[2] + t[5] + t[w3+2] + t[w3+5])>>2;
			int yval = (19595*r + 38470*g + 7471*b) >> 16;
			int uval = 128 + (((b-yval)*37028)>>16);
			int vval = 128 + (((r-yval)*46727)>>16);
			if(uval<0) uval=0;
			if(uval>255) uval=255;
			if(vval<0) vval=0;
			if(vval>255) vval=255;

			up[x>>1] = uval;
			vp[x>>1] = vval;
		}
	}
	free(temp);
}


static void draw(float time_seconds, int show)
{
#ifdef USE_FB
	struct target *front, *back;
	ping = !ping;
	front = targets+(ping ? 1 : 0);
	back = targets+(ping ? 0 : 1);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, front->framebuffer);
#endif

	initview(sizex, sizey);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);

	GLint oldprog;
	glGetIntegerv(GL_CURRENT_PROGRAM, &oldprog);
	glUseProgram(mProgram);

#ifdef USE_FB
	glBindTexture(GL_TEXTURE_2D, back->texture);
#else
	glBindTexture(GL_TEXTURE_2D, bgtexture);
#endif

	float width, height;
	width=sizex;
	height=sizey;
	glUniform1f(TimeHandle, time_seconds);
	glUniform2f(ResolutionHandle, width, height);
	glUniform2f(MouseHandle, (float)mousex/width,
			(float)(height-1-mousey)/height);
	glUniform2f(SizeHandle, 1.0, 1.0);

	glEnableVertexAttribArray(attribute_coord3d);
	glEnableVertexAttribArray(attribute_spos);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_values);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);

	GLfloat *p = vertex_data;
	float z = 0.0;
	float dx, dy;
	if(width>height)
	{
		dy = 1.0;
		dx = (float)width/height;
	} else
	{
		dx = 1.0;
		dy = (float)height/width;
	}

	dx /= zoom;
	dy /= zoom;
	glUniform2f(SizeHandle, dx, dy);
	float x = centerx - dx*.5;
	float y = centery - dy*.5;

	p[0] = -1.0;
	p[1] = -1.0;
	p[2] = z;
	p[3] = x;
	p[4] = y;

	p[5] = 1.0;
	p[6] = -1.0;;
	p[7] = z;
	p[8] = x+dx;
	p[9] = y;

	p[10] = 1.0;
	p[11] = 1.0;
	p[12] = z;
	p[13] = x+dx;
	p[14] = y+dy;

	p[15] = -1.0;
	p[16] = 1.0;
	p[17] = z;
	p[18] = x;
	p[19] = y+dy;

	glVertexAttribPointer(attribute_coord3d, 3, GL_FLOAT, GL_FALSE,
				5*sizeof(GLfloat), 0);
	glVertexAttribPointer(attribute_spos, 2, GL_FLOAT, GL_FALSE,
				5*sizeof(GLfloat), (GLvoid*)(3*sizeof(GLfloat)));

	if(timing_enabled)
		glFinish();
	uint32 tn = ugtime();
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	if(timing_enabled)
	{
		glFinish();
		tn = ugtime()-tn;
		timing_usec += tn;
		++timing_framecount;
		printf("%u %lld\n", tn, timing_usec / timing_framecount);
	}

	glUseProgram(oldprog);

#ifdef USE_FB
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	initview(sizex, sizey);
	glBindTexture(GL_TEXTURE_2D, targets[0].texture);
	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	z = 0.0;
	glTexCoord2f(0.0, 0.0);
	glVertex3f(-1.0, -1.0, z);

	glTexCoord2f(1.0, 0.0);
	glVertex3f(1.0, -1.0, z);

	glTexCoord2f(1.0, 1.0);
	glVertex3f(1.0, 1.0, z);

	glTexCoord2f(0.0, 1.0);
	glVertex3f(-1.0, 1.0, z);

	glEnd();

	glDisable(GL_TEXTURE_2D);
#else
	copy_to_back();
#endif

	if(show)
		SDL_GL_SwapBuffers();

}
