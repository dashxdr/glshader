#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int start=0;
	int end=0;
	char temp[256];

	if(argc>1)
	{
		start = atoi(argv[1]);
		end = (argc>2) ? atoi(argv[2]) : start;
	}

	int i;
	for(i=start;i<=end;++i)
	{
		snprintf(temp, sizeof(temp), "wget -q -O - http://glsl.heroku.com/item/%d", i);
		FILE *f = popen(temp, "r");
		if(!f)
		{
			printf("Error on command:\n");
			printf("%s\n", temp);
			continue;
		}
		int maxsize = 100000;
		char *buff = malloc(maxsize);
		int len = fread(buff,1, maxsize, f);
		fclose(f);
		char *put = buff;
		char *take = buff;
		char *e = take + len;
		int want = 3;
		while(take < e)
		{
			if(*take++ != '"')
				continue;
			if(--want == 0)
				break;
		}
		while(take<e)
		{
			int c = *take++;
			if(c=='\\' && take<e)
			{
				c = *take++;
				if(c=='t') c='\t';
				else if(c=='n') c='\n';
			} else if(c=='"')
				break;
			*put++ = c;
		}
//		want = 5;
//		while(put > buff)
//		{
//			if(*--put != '"')
//				continue;
//			if(--want == 0)
//				break;
//		}
		snprintf(temp, sizeof(temp), "best/%d.txt", i);
		int fd = open(temp, O_WRONLY|O_CREAT|O_TRUNC, 0644);
		if(fd>=0)
		{
			int res=write(fd, buff, put-buff);
			res=res;
			close(fd);
		}
	}
	return 0;
}
