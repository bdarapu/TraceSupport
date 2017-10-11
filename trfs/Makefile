TRFS_VERSION="0.1"

EXTRA_CFLAGS += -DTRFS_VERSION=\"$(TRFS_VERSION)\"

INC=/lib/modules/$(shell uname -r)/build/arch/x86/include

obj-$(CONFIG_TR_FS) += trfs.o

all: def

def:
	make -Wall -Werror -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules	

trfs-y := dentry.o file.o inode.o main.o super.o lookup.o mmap.o

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

