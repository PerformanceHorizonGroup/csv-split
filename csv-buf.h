/*
 * csv-buf.h
 *
 *  Created on: Jan 25, 2013
 *      Author: mike
 */

#include <stddef.h>
#include <stdlib.h>

#ifndef CSV_BUF_H_
#define CSV_BUF_H_

typedef char *cbuf;

typedef struct _cbufhdr {
	size_t size;
	size_t pos;
	char buf[];
} cbufhdr;

#define MAX_PREALLOC (1024*1024)

// Get our header
#define CBUF_HDR(p) ((cbufhdr*)(p-(sizeof(cbufhdr))))

// Member access
#define CBUF_BUF(p) (CBUF_HDR(p)->buf)
#define CBUF_LEN(p) (CBUF_HDR(p)->size)
#define CBUF_POS(p) (CBUF_HDR(p)->pos)

// Write access
#define CBUF_SETPOS(p, v) (CBUF_HDR(p)->pos=v)

#define CBUF_PTR(p) (CBUF_BUF(p)+CBUF_POS(p))
#define CBUF_REM(p) (CBUF_LEN(p)-CBUF_POS(p))

#define CBUF_PUT(p, c) (*CBUF_PTR(p)=c, CBUF_POS(p)++)

// allocation, freeing stuff
cbuf cbuf_init(size_t size);
cbuf cbuf_alloc(cbuf p, size_t size);
cbuf cbuf_double(cbuf p);
void cbuf_free(cbuf p);

// Get the buffer size
size_t cbuf_size(cbuf p);

// Put data into our buffer
cbuf cbuf_setlen(cbuf p, const char *str, size_t len);
cbuf cbuf_set(cbuf p, const char *str);
cbuf cbuf_putc(cbuf p, char c);

// Duplication
char *cbuf_dup(cbuf p, size_t *size);
char *cbuf_duplen(cbuf p, size_t *size);

#endif /* CSV_BUF_H_ */
