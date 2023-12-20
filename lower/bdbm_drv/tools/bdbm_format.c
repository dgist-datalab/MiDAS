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
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdint.h>

#include "../frontend/kernel/blkdev_ioctl.h"


struct bdbm_format_args {
	char* dev;
};

int parse_args (int argc, char** argv, struct bdbm_format_args* args)
{
	if (argc != 2)
		return -1;
	args->dev = argv[1];
	return 0;
}

int main (int argc, char** argv)
{
	struct bdbm_format_args args;
	int32_t ret = 0;
	int32_t dev_h = -1;

	fprintf (stderr, "BlueDBM format tools (Ver 0.1)\n\n");

	/* Parse arguments */
	if (parse_args (argc, argv, &args) != 0) {
		fprintf (stderr, "Usage: bdbm_format DEVICE\n");
		exit (-1);
	}

	/* Open the BDBM device */
	if ((dev_h = open (args.dev, O_RDWR)) == -1) {
		fprintf (stderr, "Failed to open '%s'\n", args.dev);
		exit (-1);
	}
	fprintf (stderr, "Open '%s' successfully\n", args.dev);

	/* Send a BDBM format command using IOCTL 
	 * Everything will be done by bdbm_drv.ko */
	ioctl (dev_h, BDBM_BADBLOCK_SCAN, 0);


	/* Wait for the BDBM device until it is formated */
	fprintf (stderr, "Progress: ");
	while (1) {
		/* check the device status every 1 second */
		fprintf (stderr, ".");
		ioctl (dev_h, BDBM_BADBLOCK_SCAN_CHECK, &ret);
		if (ret == 1)
			break;
		sleep (1);
	}
	fprintf (stderr, "\n");

	/* Display results */
	if (ret == 1) {
		fprintf (stderr, "The format has finished.\n");
	} else {
		fprintf (stderr, "The format failed (ret=%u).\n", ret);
	}

	/* close the device */
	if (dev_h != -1) {
		close (dev_h);
	}

	return 0;
}

