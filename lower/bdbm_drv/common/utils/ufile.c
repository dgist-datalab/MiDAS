/*
The MIT License (MIT)

Copyright (c) 2014-2015 CSAIL, MIT

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#if defined(KERNEL_MODE)
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>

#include "debug.h"
#include "umemory.h"
#include "ufile.h"

#elif defined(USER_MODE)
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include "ufile.h"
#include "debug.h"

#else
	#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif


#if defined(KERNEL_MODE)
bdbm_file_t bdbm_fopen (const char* path, int flags, int rights) 
{
	bdbm_file_t filp = NULL;
	mm_segment_t oldfs;
	int err = 0;

	oldfs = get_fs ();
	set_fs (get_ds ());
	filp = filp_open (path, flags, rights);
	set_fs (oldfs);
	if(IS_ERR (filp)) {
		err = PTR_ERR (filp);
		return NULL;
	}
	return filp;
}

void bdbm_fclose (bdbm_file_t file) 
{
	filp_close (file, NULL);
}

uint64_t bdbm_fread (bdbm_file_t file, uint64_t offset, uint8_t* data, uint64_t size) 
{
	mm_segment_t oldfs;
	uint64_t ret = 0;
	uint64_t len = 0;

	oldfs = get_fs ();
	set_fs (get_ds());
	while (len < size) {
		if ((ret = vfs_read (file, data + len, size - len, &offset)) == 0)
			break;
		/*if (len < size) {*/
		/*bdbm_msg ("ret=%llu, len=%llu, offset=%llu", ret, len, offset);*/
		/*}*/
		offset += ret;
		len += ret;
	}
	/*ret = vfs_read (file, data, size, &offset);*/
	set_fs (oldfs);
	return len;
} 

uint64_t bdbm_fwrite (bdbm_file_t file, uint64_t offset, uint8_t* data, uint64_t size) 
{
	mm_segment_t oldfs;
	uint64_t ret = 0;
	uint64_t len = 0;

	oldfs = get_fs ();
	set_fs (get_ds ());
	while (len < size) {
		if ((ret = vfs_write (file, data + len, size - len, &offset)) == 0)
			break;
		offset += ret;
		len += ret;
	}
	/*ret = vfs_write (file, data, size, &offset);*/
	set_fs (oldfs);
	return ret;
}

uint32_t bdbm_funlink (bdbm_file_t file)
{
	/*
	struct dentry* d = file->f_dentry->d_parent;
	struct inode* i = file->f_dentry->d_inode;
	uint32_t ret;

	if (i == NULL) {
		bdbm_msg ("i == NULL");
		return 1;
	}

	ret = vfs_unlink (i, d);
	
	return ret;
	*/

	return 0;
}

uint32_t bdbm_fsync (bdbm_file_t file) 
{
	return vfs_fsync (file, 0);
}

void bdbm_flog (const char* filename, char* string)
{
	bdbm_file_t fp = NULL;

	if ((fp = bdbm_fopen (filename, O_CREAT | O_WRONLY | O_APPEND, 0777)) == NULL) {
		bdbm_error ("bdbm_fopen failed");
		return;
	}

	bdbm_fwrite (fp, 0, string, strlen (string));
	bdbm_fclose (fp);
}

#elif defined(USER_MODE)

bdbm_file_t bdbm_fopen (const char* path, int flags, int rights) 
{
	return open (path, flags, rights);
}

void bdbm_fclose (bdbm_file_t file) 
{
	close (file);
}

uint64_t bdbm_fread (bdbm_file_t file, uint64_t offset, uint8_t* data, uint64_t size) 
{
	uint64_t ret = 0;
	uint64_t len = 0;

	lseek (file, offset, SEEK_SET);
	while (len < size) {
		if ((ret = read (file, data + len, size - len)) == 0)
			break;
		len += ret;
	}
	return len;
} 

uint64_t bdbm_fwrite (bdbm_file_t file, uint64_t offset, uint8_t* data, uint64_t size) 
{
	uint64_t ret = 0;
	uint64_t len = 0;

	lseek (file, offset, SEEK_SET);
	while (len < size) {
		if ((ret = write (file, data + len, size - len)) == 0)
			break;
		len += ret;
	}
	return len;
}

uint32_t bdbm_funlink (bdbm_file_t file)
{
	/*return unlink (file);*/
	return 0;
}

uint32_t bdbm_fsync (bdbm_file_t file) 
{
	return syncfs (file);
}

void bdbm_flog (const char* filename, char* string)
{
	bdbm_file_t fp = 0;

	if ((fp = bdbm_fopen (filename, O_CREAT | O_WRONLY | O_APPEND, 0777)) == 0) {
		/*bdbm_error ("bdbm_fopen failed");*/
		return;
	}

	bdbm_fwrite (fp, 0, (uint8_t*)string, strlen (string));
	bdbm_fclose (fp);
}

#else
	#error Invalid Platform (KERNEL_MODE or USER_MODE)
#endif
