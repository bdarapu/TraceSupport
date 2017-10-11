#ifndef TRFSCTL_H
#define TRFSCTL_H

#define MAGIC_NUMBER  	0x7865FF

/* List of values */
#define BITMAP_GET_VALUE 		_IOR(MAGIC_NUMBER, 0, int)
#define BITMAP_ALL_VALUE 		_IOW(MAGIC_NUMBER, 1, int)
#define BITMAP_NONE_VALUE 		_IOW(MAGIC_NUMBER, 2, int)
#define BITMAP_HEX_VALUE	    _IOW(MAGIC_NUMBER, 3, int)

#endif
