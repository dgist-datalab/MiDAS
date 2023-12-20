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

#ifndef _BLUEDBM_IOCTL_H
#define _BLUEDBM_IOCTL_H

#define BDBM_BADBLOCK_SCAN _IOR(0, 0, int)
#define BDBM_BADBLOCK_SCAN_CHECK _IOR(0, 1, int)
/*#define BDBM_GET_PHYADDR _IOR(0, 2, struct phyaddr)*/

#ifdef MODULE
/* kernel module */

#ifndef HDIO_GETGEO_BIG /* for backward-compatibility */
#define HDIO_GETGEO_BIG	0x330
#endif

#ifndef HDIO_GETGEO_BIG_RAW
#define HDIO_GETGEO_BIG_RAW	0x331
#endif

int bdbm_blk_ioctl (struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg);
int bdbm_blk_getgeo (struct block_device *bdev, struct hd_geometry* geo);

#endif	/* MODULE */

#endif  /* _BLUEDBM_IOCTL_H */
