#ifndef _MINI_GZIP_H_
#define _MINI_GZIP_H_

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

/* mini_gzip.c */
extern int	mini_gz_start(struct mini_gzip *gz_ptr, void *mem, size_t mem_len);
extern void	mini_gz_chunksize_set(struct mini_gzip *gz_ptr, int chunk_size);
extern void	mini_gz_init(struct mini_gzip *gz_ptr);
extern int	mini_gz_unpack(struct mini_gzip *gz_ptr, void *mem_out, size_t mem_out_len);

#endif
