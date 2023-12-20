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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h> 
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <linux/fs.h>


extern uint8_t* blk_buf;

int sw_comp_display (const int fd_dev, const char* pattern, const int start_blk, const int len)
{
	printf ("%15d\t%15d\t%15d\n", start_blk, start_blk + len - 1, len);
	return 0;
}

int sw_comp (const int fd_dev, const char* pattern, const int start_blk, const int blks)
{
	int match = 0;
	off64_t ofs = 0, len = 0;

	ofs = start_blk;
	ofs = ofs * 4096;
	if (lseek64 (fd_dev, ofs, SEEK_SET) < 0) {
		fprintf (stderr, "lseek failed: start_blk=%d %s\n", start_blk, strerror (errno));
		return -1;
	}

	ofs = 0;
	len = blks * 4096;
	while (ofs < len) {
		ofs += read (fd_dev, blk_buf + ofs, len - ofs);
	}

	for (ofs = 0; ofs <= len - strlen (pattern); ofs++) {
		if (bcmp (blk_buf + ofs, pattern, strlen (pattern)) == 0) {
			match++;
		}
	}

	return match;
}

