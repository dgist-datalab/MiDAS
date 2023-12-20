#include "uilog.h"

#if defined (USER_MODE)

int ilog2 (int x) {
	int b1, b2, b3, b4, b5;

	/* pass the most significant 1 all the way down */
	/* shamely steal from here:
	 * * http://aggregate.org/MAGIC/#Most%20Significant%201%20Bit */
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >> 16);

	/*count the 1s now */

	/* b1 = 0x55555555 */
	b1 = 0x55 | (0x55 << 8); 
	b1 = b1 | (b1 << 16);

	/* b2 = 0x33333333 */
	b2 = 0x33 | (0x33 << 8);
	b2 = b2 | (b2 << 16);

	/* b3 = 0x0f0f0f0f */
	b3 = 0x0f | (0x0f << 8);
	b3 = b3 | (b3 << 16);

	/* b4 = 0x00ff00ff */
	b4 = 0xff | (0xff << 16);

	/* b5 = 0x0000ffff */
	b5 = 0xff | (0xff << 8);

	/*divide and conquer */
	x = (x & b1) + ((x >> 1) & b1);
	x = (x & b2) + ((x >> 2) & b2);
	x = (x & b3) + ((x >> 4) & b3);
	x = (x & b4) + ((x >> 8) & b4);
	x = (x & b5) + ((x >> 16) & b5);

	x = x + ~0;
	return x;
}

#endif
