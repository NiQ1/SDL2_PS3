/*
 * jmemansi.c
 *
 * Copyright (C) 1992-1996, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file provides a simple generic implementation of the system-
 * dependent portion of the JPEG memory manager.  This implementation
 * assumes that you have the ANSI-standard library routine tmpfile().
 * Also, the problem of determining the amount of memory available
 * is shoved onto the user.
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "jmemsys.h"		/* import the system-dependent declarations */

#ifndef HAVE_STDLIB_H		/* <stdlib.h> should declare malloc(),free() */
extern void * malloc JPP((size_t size));
extern void free JPP((void *ptr));
#endif

#ifndef SEEK_SET		/* pre-ANSI systems may not define this; */
#define SEEK_SET  0		/* if not, assume 0 is correct */
#endif

#ifdef __PS3__
#include <sys/paths.h>

struct _PS3_TMPFILE_RECORD;
typedef struct _PS3_TMPFILE_RECORD PS3_TMPFILE_RECORD;
struct _PS3_TMPFILE_RECORD
{
	char* file_name;
	FILE* fd;
	PS3_TMPFILE_RECORD* next;
};
typedef struct _PS3_TMPFILES
{
	PS3_TMPFILE_RECORD* first;
} PS3_TMPFILES;

PS3_TMPFILES g_ps3_tmpfiles = { NULL };
int g_ps3_tmp_init = 0;

int ps3_tmp_is_in_list(const PS3_TMPFILES* ls, const char* fn)
{
	PS3_TMPFILE_RECORD* current = NULL;

	if (ls == NULL) {
		return 0;
	}
	if (ls->first == NULL) {
		return 0;
	}
	current = ls->first;
	while (current != NULL) {
		if (strcmp(current->file_name, fn) == 0) {
			return 1;
		}
	}
	return 0;
}

const char* ps3_tmp_get_fn(const PS3_TMPFILES* ls, const FILE* fd)
{
	PS3_TMPFILE_RECORD* current = NULL;

	if (ls == NULL || fd == NULL) {
		return NULL;
	}
	current = ls->first;
	while (current != NULL || current->fd != fd) {
		current=current->next;
	}
	if (current == NULL) {
		return NULL;
	}
	return current->file_name;
}

// fn needs to be at least 13 bytes in size
void ps3_tmp_gen_filename(const PS3_TMPFILES* ls, char* fn, size_t fn_buf_size)
{
	int i = 0;
	size_t dir_size = sizeof(TEMP_DIRECTORY "/");
	size_t fn_size = dir_size + 13;

	if (fn_buf_size < fn_size) {
		return;
	}
	strncpy(fn, TEMP_DIRECTORY "/", fn_buf_size);
	// Make sure filename is unique
	do {

		// Generate 8 random lowercase chars
		for (i=0; i<8; i++) {
			fn[dir_size+i] = 'a' + (rand() % 25);
		}
		// Extension is always ".tmp"
		fn[dir_size+8]='.';
		fn[dir_size+9]='t';
		fn[dir_size+10]='m';
		fn[dir_size+11]='p';
		fn[dir_size+12]='\0';
	} while (ps3_tmp_is_in_list(ls, fn));
}

int ps3_tmp_insert(PS3_TMPFILES* ls, char* fn, FILE* fd)
{
	PS3_TMPFILE_RECORD* newfile = NULL;
	PS3_TMPFILE_RECORD* current = NULL;

	if (ls == NULL) {
		return -1;
	}
	newfile = (PS3_TMPFILE_RECORD*)malloc(sizeof(PS3_TMPFILE_RECORD));
	if (newfile == NULL) {
		return -1;
	}
	newfile->file_name = fn;
	newfile->fd = fd;
	newfile->next = NULL;
	if (ls->first == NULL) {
		// List is empty, insert first entry
		ls->first = newfile;
		return 0;
	}
	current = ls->first;
	while (current->next != NULL) {
		current = current->next;
	}
	current->next = newfile;
	return 0;
}

int ps3_tmp_remove(PS3_TMPFILES* ls, FILE* fd)
{
	PS3_TMPFILE_RECORD* current = NULL;
	PS3_TMPFILE_RECORD* next = NULL;

	if (ls == NULL || fd == NULL) {
		return -1;
	}
	if (ls->first == NULL) {
		// List is empty
		return -1;
	}
	if (ls->first->fd == fd) {
		// Requested item is the first in the list
		ls->first = ls->first->next;
		return 0;
	}
	current = ls->first;
	while (current->next != NULL && current->next->fd != fd) {
		current = current->next;
	}
	if (current->next == NULL) {
		// Not found
		return -1;
	}
	next = current->next->next;
	free(current->next->file_name);
	free(current->next);
	current->next = next;
	return 0;
}

void ps3_tmp_close(FILE* fd)
{
	fclose(fd);
	const char* fn = ps3_tmp_get_fn(&g_ps3_tmpfiles, fd);
	if (fn == NULL) {
		return;
	}
	remove(fn);
	ps3_tmp_remove(&g_ps3_tmpfiles, fd);
}

void ps3_tmp_close_all()
{
	PS3_TMPFILE_RECORD* current = g_ps3_tmpfiles.first;
	PS3_TMPFILE_RECORD* next = NULL;
	while (current != NULL) {
		next = current->next;
		fclose(current->fd);
		remove(current->file_name);
		free(current->file_name);
		free(current);
		current = next;
	}
}

FILE* ps3_tmpfile()
{
	int dir_size = sizeof(TEMP_DIRECTORY "/");
	int fn_size = dir_size + 15;
	if (!g_ps3_tmp_init) {
		atexit(ps3_tmp_close_all);
		g_ps3_tmp_init = 1;
	}
	// Generate temp name
	char* full_fn = (char*)malloc(fn_size);
	FILE* fd = NULL;
	if (full_fn == NULL) {
		// Out of memory
		return NULL;
	}
	ps3_tmp_gen_filename(&g_ps3_tmpfiles, full_fn, fn_size);
	// Attempt to create the temporary file
	fd = fopen(full_fn, "w+b");
	if (fd == NULL) {
		free(full_fn);
		return NULL;
	}
	// Insert into temp files list
	if (ps3_tmp_insert(&g_ps3_tmpfiles, full_fn+dir_size, fd) < 0) {
		// Failed
		fclose(fd);
		remove(full_fn);
		free(full_fn);
		return NULL;
	}
	return fd;
}

#define tmpfile ps3_tmpfile
#define tmp_close ps3_tmp_close

#else

#define tmp_close fclose

#endif

/*
 * Memory allocation and freeing are controlled by the regular library
 * routines malloc() and free().
 */

GLOBAL(void *)
jpeg_get_small (j_common_ptr cinfo, size_t sizeofobject)
{
  return (void *) malloc(sizeofobject);
}

GLOBAL(void)
jpeg_free_small (j_common_ptr cinfo, void * object, size_t sizeofobject)
{
  free(object);
}


/*
 * "Large" objects are treated the same as "small" ones.
 * NB: although we include FAR keywords in the routine declarations,
 * this file won't actually work in 80x86 small/medium model; at least,
 * you probably won't be able to process useful-size images in only 64KB.
 */

GLOBAL(void FAR *)
jpeg_get_large (j_common_ptr cinfo, size_t sizeofobject)
{
  return (void FAR *) malloc(sizeofobject);
}

GLOBAL(void)
jpeg_free_large (j_common_ptr cinfo, void FAR * object, size_t sizeofobject)
{
  free(object);
}


/*
 * This routine computes the total memory space available for allocation.
 * It's impossible to do this in a portable way; our current solution is
 * to make the user tell us (with a default value set at compile time).
 * If you can actually get the available space, it's a good idea to subtract
 * a slop factor of 5% or so.
 */

#ifndef DEFAULT_MAX_MEM		/* so can override from makefile */
#define DEFAULT_MAX_MEM		1000000L /* default: one megabyte */
#endif

GLOBAL(long)
jpeg_mem_available (j_common_ptr cinfo, long min_bytes_needed,
		    long max_bytes_needed, long already_allocated)
{
  return cinfo->mem->max_memory_to_use - already_allocated;
}


/*
 * Backing store (temporary file) management.
 * Backing store objects are only used when the value returned by
 * jpeg_mem_available is less than the total space needed.  You can dispense
 * with these routines if you have plenty of virtual memory; see jmemnobs.c.
 */


METHODDEF(void)
read_backing_store (j_common_ptr cinfo, backing_store_ptr info,
		    void FAR * buffer_address,
		    long file_offset, long byte_count)
{
  if (fseek(info->temp_file, file_offset, SEEK_SET))
    ERREXIT(cinfo, JERR_TFILE_SEEK);
  if (JFREAD(info->temp_file, buffer_address, byte_count)
      != (size_t) byte_count)
    ERREXIT(cinfo, JERR_TFILE_READ);
}


METHODDEF(void)
write_backing_store (j_common_ptr cinfo, backing_store_ptr info,
		     void FAR * buffer_address,
		     long file_offset, long byte_count)
{
  if (fseek(info->temp_file, file_offset, SEEK_SET))
    ERREXIT(cinfo, JERR_TFILE_SEEK);
  if (JFWRITE(info->temp_file, buffer_address, byte_count)
      != (size_t) byte_count)
    ERREXIT(cinfo, JERR_TFILE_WRITE);
}


METHODDEF(void)
close_backing_store (j_common_ptr cinfo, backing_store_ptr info)
{
  tmp_close(info->temp_file);
  /* Since this implementation uses tmpfile() to create the file,
   * no explicit file deletion is needed.
   */
}


/*
 * Initial opening of a backing-store object.
 *
 * This version uses tmpfile(), which constructs a suitable file name
 * behind the scenes.  We don't have to use info->temp_name[] at all;
 * indeed, we can't even find out the actual name of the temp file.
 */

GLOBAL(void)
jpeg_open_backing_store (j_common_ptr cinfo, backing_store_ptr info,
			 long total_bytes_needed)
{
  if ((info->temp_file = tmpfile()) == NULL)
    ERREXITS(cinfo, JERR_TFILE_CREATE, "");
  info->read_backing_store = read_backing_store;
  info->write_backing_store = write_backing_store;
  info->close_backing_store = close_backing_store;
}


/*
 * These routines take care of any system-dependent initialization and
 * cleanup required.
 */

GLOBAL(long)
jpeg_mem_init (j_common_ptr cinfo)
{
  return DEFAULT_MAX_MEM;	/* default for max_memory_to_use */
}

GLOBAL(void)
jpeg_mem_term (j_common_ptr cinfo)
{
  /* no work */
}
