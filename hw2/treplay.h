#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct lookup{
	int key;
	int fd;
}lookup;

typedef struct open_struct{
	unsigned int flags;
	unsigned short mode;
	unsigned short pathname_length;
	char *pathname;
	int errno;
	int retval;
	
}open_struct;

typedef struct read_struct{
	int record_id_open;
	char * buf;
	size_t user_bytes; // number of bytes to be read entered by user
	int errno; //return value from read call
	int fd;
	int num_bytes;//number of bytes read in treplay 
	char *trace_buf;
	
}read_struct;

typedef struct write_struct{
	int record_id_open;
	size_t count;//number of bytes entered by user
	char * buf;
	int errno;//return value from trfs write
	int fd;
	int num_bytes; //number of bytes in trace 
	
}write_struct;

typedef struct close_struct{
	int record_id_open;
	int fd;
	int retval; // value returned at trace 
}close_struct;

typedef struct mkdir_struct{
	unsigned short mode;
	unsigned short path_size;
	char * path;
	int errno;
	int retval;
}mkdir_struct;

typedef struct rmdir_struct{
	unsigned short path_size;
	char * path;
	int errno;
	int retval;
}rmdir_struct;


