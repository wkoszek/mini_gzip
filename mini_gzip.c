/*
 * BSD 2-clause license
 * Copyright (c) 2013 Wojciech A. Koszek <wkoszek@FreeBSD.org>
 * 
 * Based on:
 * 
 * https://github.com/strake/gzip.git
 *
 * I had to rewrite it, since strake's version was powered by UNIX FILE* API,
 * while the key objective was to perform memory-to-memory operations
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "miniz.h"

#define MAX_PATH_LEN		1024
#define	MINI_GZ_MIN(a, b)	((a) < (b) ? (a) : (b))

struct mini_gzip {
	size_t		total_len;
	size_t		data_len;
	size_t		chunk_size;

	uint32_t	magic;
#define	MINI_GZIP_MAGIC	0xbeebb00b

	uint16_t	fcrc;
	uint16_t	fextra_len;

	uint8_t		*hdr_ptr;
	uint8_t		*fextra_ptr;
	uint8_t		*fname_ptr;
	uint8_t		*fcomment_ptr;

	uint8_t		*data_ptr;
	uint8_t		pad[3];
};

int
mini_gz_start(struct mini_gzip *gz_ptr, void *mem, size_t mem_len)
{
	uint8_t		*hptr, *hauxptr, *mem8_ptr;
	uint16_t	fextra_len;

	assert(gz_ptr != NULL);

	mem8_ptr = (uint8_t *)mem;
	hptr = mem8_ptr + 0;		// .gz header
	hauxptr = mem8_ptr + 10;	// auxillary header

	gz_ptr->hdr_ptr = hptr;
	gz_ptr->data_ptr = 0;
	gz_ptr->data_len = 0;
	gz_ptr->total_len = mem_len;
	gz_ptr->chunk_size = 1024;

	if (hptr[0] != 0x1F || hptr[1] != 0x8B) {
		printf("hptr[0] = %02x hptr[1] = %02x\n", hptr[0], hptr[1]);
		return (-1);
	}
	if (hptr[2] != 8) {
		return (-2);
	}
	if (hptr[3] & 0x4) {
		fextra_len = hauxptr[1] << 8 | hauxptr[0];
		gz_ptr->fextra_len = fextra_len;
		hauxptr += 2;
		gz_ptr->fextra_ptr = hauxptr;
	}
	if (hptr[3] & 0x8) {
		gz_ptr->fname_ptr = hauxptr;
		while (*hauxptr != '\0') {
			hauxptr++;
		}
		hauxptr++;
	}
	if (hptr[3] & 0x10) {
		gz_ptr->fcomment_ptr = hauxptr;
		while (*hauxptr != '\0') {
			hauxptr++;
		}
		hauxptr++;
	}
	if (hptr[3] & 0x2) /* FCRC */ {
		gz_ptr->fcrc = (*(uint16_t *)hauxptr);
		hauxptr += 2;
	}
	gz_ptr->data_ptr = hauxptr;
	gz_ptr->data_len = mem_len - (hauxptr - hptr);
	gz_ptr->magic = MINI_GZIP_MAGIC;
	return (0);
}

void
mini_gz_chunksize_set(struct mini_gzip *gz_ptr, int chunk_size)
{

	assert(gz_ptr != 0);
	assert(gz_ptr->magic == MINI_GZIP_MAGIC);
	gz_ptr->chunk_size = chunk_size;
}

void
mini_gz_init(struct mini_gzip *gz_ptr)
{

	memset(gz_ptr, 0xffffffff, sizeof(*gz_ptr));
	gz_ptr->magic = MINI_GZIP_MAGIC;
	mini_gz_chunksize_set(gz_ptr, 1024);
}


int
mini_gz_unpack(struct mini_gzip *gz_ptr, void *mem_out, size_t mem_out_len)
{
	z_stream s;
	int	ret, in_bytes_avail, bytes_to_read;

	assert(gz_ptr != 0);
	assert(gz_ptr->data_len > 0);
	assert(gz_ptr->magic == MINI_GZIP_MAGIC);

	memset (&s, 0, sizeof (z_stream));
	inflateInit2(&s, -MZ_DEFAULT_WINDOW_BITS);
	in_bytes_avail = gz_ptr->data_len;
	s.avail_out = mem_out_len;
	s.next_in = gz_ptr->data_ptr;
	s.next_out = mem_out;
	for (;;) {
		bytes_to_read = MINI_GZ_MIN(gz_ptr->chunk_size, in_bytes_avail);
		s.avail_in += bytes_to_read;
		ret = mz_inflate(&s, MZ_SYNC_FLUSH);
		in_bytes_avail -= bytes_to_read;
		if (s.avail_out == 0 && in_bytes_avail != 0) {
			return (-3);
		}
		assert(ret != MZ_BUF_ERROR);
		if (ret == MZ_PARAM_ERROR) {
			return (-1);
		}
		if (ret == MZ_DATA_ERROR) {
			return (-2);
		}
		if (ret == MZ_STREAM_END) {
			break;
		}
	}
	ret = inflateEnd(&s);
	if (ret != Z_OK) {
		return (-4);
	}
	return (s.total_out);
}

#ifdef TEST_PROG

#define	GAS(comp, ...)	do {					\
	if (!((comp))) {						\
		fprintf(stderr, "Error: ");				\
		fprintf(stderr, __VA_ARGS__);				\
		fprintf(stderr, ", %s:%d\n", __func__, __LINE__);	\
		fflush(stderr);						\
		exit(1);						\
	}								\
} while (0)

int
main(int argc, char **argv)
{
	struct mini_gzip gz;
	struct stat st;
	char	in_fn[MAX_PATH_LEN], out_fn[MAX_PATH_LEN];
	char	*sptr, *mem_in, *mem_out;
	int	level, flag_c, o, in_fd, out_fd, ret, is_gzipped, out_len;

	level = 6;
	flag_c = 0;
	while ((o = getopt(argc, argv, "cd123456789")) != -1) {
		switch (o) {
		case 'd':
			level = 0;
			break;
		case 'c':
			flag_c = 1;
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			assert(o != '0');
			level = o - '0';
			break;
		default:
			abort();
			break;
		}
	}
	
	argc -= optind;
	argv += optind;

	GAS(argc == 2, "2 file names must be passed: input and output file");

	is_gzipped = 0;
	sptr = strstr(argv[0], ".gz");
	if (sptr != NULL) {
		is_gzipped = 1;
	}
	printf("flag_c: %d is_gzipped: %d\n", flag_c, is_gzipped);

	if (is_gzipped) {
		GAS(flag_c == 0, "Requesting to compress .gz file? Looks wrong");
	} else {
		GAS(flag_c == 1, "Requesting to decompress normal file?");
	}

	snprintf(in_fn, sizeof(in_fn) - 1, "%s", argv[0]);
	snprintf(out_fn, sizeof(out_fn) - 1, "%s", argv[1]);

	printf("in_fn: %s out_fn: %s level %d\n", in_fn, out_fn, level);

	mem_in = calloc(1024*1024, 1);
	mem_out = calloc(1024*1024, 1);
	GAS(mem_in != NULL, "Couldn't allocate memory for input file");
	GAS(mem_out != NULL, "Couldn't allocate memory for output file");

	in_fd = open(in_fn, O_RDONLY);
	GAS(in_fd != -1, "Couldn't open file %s for reading", in_fn);
	ret = fstat(in_fd, &st);
	GAS(ret == 0, "Couldn't call fstat(), %d returned", ret);
	ret = read(in_fd, mem_in, st.st_size);
	GAS(ret == st.st_size, "Read only %d bytes, %jd expected", ret,
							(uintmax_t)st.st_size);
	out_fd = open(out_fn, O_WRONLY|O_CREAT, st.st_mode);
	GAS(out_fd != -1, "Couldn't create output file '%s' for writing",
								out_fn);

	if (flag_c) {
		abort();
#if 0
		mini_gz_init(&gz);
		printf("COMP\n");
		out_len = mini_gz_pack(&gz, level, mem_in, st.st_size, mem_out,
								1024*1024);
		printf("out_len = %d\n", out_len);
		ret = write(out_fd, mem_out, out_len);
		printf("ret = %d\n", ret);
		GAS(ret == out_len, "Wrote only %d bytes, expected %d", ret, out_len);
#endif
	} else {
		printf("--- testing decompression --\n");
		ret = mini_gz_start(&gz, mem_in, st.st_size);
		GAS(ret == 0, "mini_gz_start() failed, ret=%d", ret);
		out_len = mini_gz_unpack(&gz, mem_out, 1024*1024);
		printf("out_len = %d\n", out_len);
		ret = write(out_fd, mem_out, out_len);
		printf("ret = %d\n", ret);
		GAS(ret == out_len, "Wrote only %d bytes, expected %d", ret, out_len);
	}
	close(in_fd);
	close(out_fd);

	return 0;
}
#endif
