#ifndef _FILEIO_H
#define _FILEIO_H

#include <types.h>
#include <kernel/vfs.h>

#define MAX_OPEN_FILES 128

typedef struct open_file {
	dev_t dev;
	ino_t ino;
	off_t offset;
	off_t size;
} open_file_t;

#endif