#ifndef IOCT_H
#define IOCT_H

#include <linux/ioctl.h>
#define MAGIC_NO 89

#define SET_IOCTL _IOR(MAGIC_NO, 1, int)
#define LIST_VECTOR _IOR(MAGIC_NO, 3, int)
#define LIST_ID _IOR(MAGIC_NO, 4, int)
#define REMOVE_IOCTL _IOR(MAGIC_NO, 2, int)

struct param{
  int v_id;
  int p_id;
};

void dummy(void);

#endif
