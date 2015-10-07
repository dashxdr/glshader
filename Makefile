CFLAGS = -O2 -Wall $(shell sdl-config --cflags)
#LDFLAGS = $(shell sdl-config --libs) -lGL -lm -lavutil -lavformat -lavcodec
#LDLIBS = $(shell sdl-config --libs) -lGL -lm
LDLIBS = -lSDL -lGL -lm

PCP=/opt/ffmpeg-20150915/lib/pkgconfig

FFMPEG_LIBS=    libavdevice                        \
                libavformat                        \
                libavfilter                        \
                libavcodec                         \
                libswresample                      \
                libswscale                         \
                libavutil                          \


CFLAGS := $(shell PKG_CONFIG_PATH=$(PCP) pkg-config --cflags $(FFMPEG_LIBS)) $(CFLAGS)
LDLIBS := $(LDLIBS) $(shell PKG_CONFIG_PATH=$(PCP) pkg-config --libs $(FFMPEG_LIBS))


all: glshader getall

glshader:	main.o encoder.o
	$(CC) $(LDFLAGS) -o $@ $^  $(LDLIBS)
getall: getall.c

main.o: main.c

encoder.o: encoder.c

clean:
	rm -f glshader getall *.o

test: glshader
	@#./glshader best/*.txt
	./glshader -o test.mp4 best/27314.txt
	mplayer test.mp4
	mediainfo test.mp4

WORK = /ram
VER = 1.0.0
DDIR = glshader-$(VER)

package:  clean
	rm -rf $(WORK)/$(DDIR)
	mkdir $(WORK)/$(DDIR)
	cp *.c *.h Makefile* $(WORK)/$(DDIR)
	#cp *.h *.m $(WORK)/$(DDIR)
	#cp -a data $(WORK)/$(DDIR)
	#cp ChangeLog README $(WORK)/$(DDIR)
	cd $(WORK) && tar czf $(DDIR).tgz $(DDIR)
	rm -rf $(WORK)/$(DDIR)
