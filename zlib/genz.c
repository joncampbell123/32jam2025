
#include <zlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

int main(int argc,char **argv) {
	unsigned char *buf2;
	unsigned char *buf;
	const char *file;
	unsigned int dsz;
	z_stream z;
	off_t sz;
	int fd;

	file = argv[1];
	if (!file) return 1;

	fd = open(file,O_RDONLY|O_BINARY);
	if (fd < 0) return 1;

	sz = lseek(fd,0,SEEK_END);
	if (sz <= 0 || sz > 4096) return 1;
	if (lseek(fd,0,SEEK_SET) != 0) return 1;

	buf = malloc((size_t)sz);
	if (!buf) return 1;

	if (read(fd,buf,(size_t)sz) != (size_t)sz) return 1;

	buf2 = malloc((size_t)sz);
	if (!buf2) return 1;

	memset(&z,0,sizeof(z));
	z.next_in = buf;
	z.avail_in = (unsigned int)sz;
	z.next_out = buf2;
	z.avail_out = (unsigned int)sz;
	z.data_type = Z_BINARY;
	if (deflateInit(&z,8) != Z_OK) return 1;
	if (deflate(&z,Z_FINISH) != Z_STREAM_END) return 1;
	if (deflateEnd(&z) != Z_OK) return 1;

	dsz = (unsigned int)z.total_out;
	printf("static const unsigned char _zlib_data[%u] = {\n",dsz);
	{
		const unsigned char *p = buf2;
		unsigned int x=0,i;

		for (i=0;i < dsz;i++) {
			if (x == 0) printf("/*%04u*/ ",i);
			printf("0x%02X",buf2[i]);
			if ((i+1) != dsz) printf(",");
			if ((++x) == 16) {
				printf("\n");
				x = 0;
			}
		}
		if (x) printf("\n");
	}
	printf("};\n");

	free(buf2);
	free(buf);
	return 0;
}

