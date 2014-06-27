CFLAGS = -O2 -Wall $(shell sdl-config --cflags)
#LDFLAGS = $(shell sdl-config --libs) -lGL -lm -lavutil -lavformat -lavcodec
LDFLAGS = $(shell sdl-config --libs) -lGL -lm

all: glshader getall

glshader:	main.o # encoder.o
	$(CC) -o $@ $^ $(LDFLAGS)
getall: getall.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

main.o: main.c

encoder.o: encoder.c

clean:
	rm -f glshader getall *.o

test: glshader
	./glshader best/*.txt


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
