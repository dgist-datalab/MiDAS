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

#include <string.h>
#include <stdlib.h>

#include "debug.h"

void* bdbm_malloc (size_t size) { return calloc (1, size); }
void* bdbm_malloc_atomic (size_t size) { return calloc (1, size); }
void* bdbm_zmalloc (size_t size) { return calloc (1, size); }
void bdbm_free (void* addr) { free (addr); }
void bdbm_free_atomic (void* addr) { free (addr); }
void* bdbm_memcpy (void* dst, void* src, size_t size) { return memcpy (dst, src, size); }
void* bdbm_memset (void* addr, int c, size_t size) { return memset (addr, c, size); }

void* bdbm_malloc_phy (size_t size) { 
	static int i = 0;
	bdbm_msg ("bdbm_malloc_phy () is called: %d", i++);
	return calloc (1, size); 
}

void bdbm_free_phy (void* addr) { 
	static int i = 0;
	bdbm_msg ("bdbm_free_phy () is called: %d", i++);
	free (addr); 
}
