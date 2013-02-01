/*
 * csv-buf.c
 *
 *  Created on: Jan 25, 2013
 *      Author: mike
 */

#include "csv-buf.h"
#include <string.h>
#include <stdio.h>

// Initialize our buffer
cbuf cbuf_init(size_t size) {
	// Allocate memory for our buffer
	cbufhdr *ch = malloc(sizeof(cbufhdr)+size+1);

	// OOM sanity check
	if(!ch) return NULL;

	// Set up our size and current position
	ch->size = size;
	ch->pos = 0;

	// Return our buffer
	return (char*)ch->buf;
}

// Free our buffer
void cbuf_free(cbuf buf) {
	if(buf == NULL) return;
	free(buf-sizeof(cbufhdr));
}

// Allocate enough room for size bytes
cbuf cbuf_alloc(cbuf buf, size_t size) {
	cbufhdr *ch = (void*)(buf-sizeof(cbufhdr)), *newch;

	// No need to allocate if we have enough room
	if(size < ch->size) return buf;

	// Actually allocate double the space requested if small enough
	if(size < MAX_PREALLOC) {
		size *= 2;
	}

	// Reallocate our buffer
	newch = realloc(ch, sizeof(cbufhdr) + size + 1);
	newch->size = size;

	// Return our new buffer
	return (char*)newch->buf;
}

// Double the size of our buffer
cbuf cbuf_double(cbuf buf) {
	return cbuf_alloc(buf, cbuf_size(buf)*2);
}

// Get the size of our buffer
size_t cbuf_size(cbuf buf) {
	cbufhdr *ch = (cbufhdr*)(buf-sizeof(cbufhdr));
	return ch->size;
}

// Set a buffer outright
cbuf cbuf_setlen(cbuf buf, const char *s, size_t len) {
	// Sanity checks
	if(!buf || !s || len < 0) return NULL;

    // Make sure we have enough room
    buf = cbuf_alloc(buf, len);

    // Copy in string, null terminate
    memcpy(buf, s, len);
    buf[len]='\0';
    
	// Return our buffer
	return buf;
}

// Set to a given string
cbuf cbuf_set(cbuf buf, const char *s) {
	return cbuf_setlen(buf, s, strlen(s));
}

// Put a character a the end of our output buffer
cbuf cbuf_putc(cbuf buf, char c) {
	if(!CBUF_REM(buf)) buf = cbuf_double(buf);
	CBUF_PUT(buf, c);
	return buf;
}

// Return a copy of the buffer and it's size
char *cbuf_dup(cbuf buf, size_t *size) {
	if(!buf) return NULL;

	// Allocate memory, set size, copy it in
	char *ret = (char*)malloc(CBUF_POS(buf));
	*size = CBUF_POS(buf);
	memcpy(ret, buf, CBUF_POS(buf));

	// return our buffer
	return ret;
}

// Return a copy of len bytes of our queue
char *cbuf_duplen(cbuf buf, size_t *len) {
    if(!buf) return NULL;

    // Don't return more bytes than we've got
    if(*len > CBUF_POS(buf)) {
        *len = CBUF_POS(buf);
    }

    char *ret = (char*)malloc(*len);
    memcpy(ret, buf, *len);

    // Return our data
    return ret;
}
