struct myframe {
	unsigned char *data[4];
	int linesize[4];
	float pts; // in seconds

	int width, height;
};

int encode_my_video(char *outputname, int width, int height,  int numframes,
		float framerate);


void draw_frame(struct myframe *f, int halfx, int halfy);
