
#include <zlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(int argc,char **argv) {
	const char *s = argv[1] ? argv[1] : "";
	const unsigned int l = strlen(s);
	uint32_t a = adler32(0,NULL,0);
	uint32_t c = crc32(0,NULL,0);
	printf("/* string, len, adler32 init, crc32 init, adler32 final, crc32 final */\n");
	printf("{\"%s\",%u,0x%08lx,0x%08lx",s,l,a,c);
	if (l) {
		a = adler32(a,s,l);
		c = crc32(c,s,l);
	}
	printf(",0x%08lx,0x%08lx",a,c);
	printf("}\n");
	return 0;
}

