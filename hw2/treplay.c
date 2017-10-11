#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <asm/unistd.h>
#include <sys/syscall.h>

#include "treplay.h"

#define mode_default 0
#define mode_n 1
#define mode_s 2


int main(int argc, char *argv[])
{
   int stream;
   char *buffer;
   int c;
   char * filename;
   char *ptr;
   

    char buf_record_size[2];
	unsigned short record_size=0;
	int record_id=0;
	char record_type;
	int mode=mode_default;
	lookup lookup_arr[500];
	open_struct open1;
	write_struct write1;
	read_struct read1;
	close_struct close1;
	mkdir_struct mkdir1;
	rmdir_struct rmdir1;
	char ch;
	int retval;
	int lookup_index=0;
	int i;
	
	//getopt for parsing -s or -n option
	while ((c = getopt (argc, argv, "ns")) != -1)
    switch (c)
      {
      case 'n':
	  if(mode==mode_s)
	  {
		  printf("options -n or -s not -n and -s \n");
		  exit(0);
	  }
        mode=mode_n;
        break;
      case 's':
        if(mode==mode_n)
		{
			printf("options -n or -s not -n and -s \n");
			exit(0);
		}
		mode=mode_s;
        break;
	  
	  case '?':
		printf("Usage : ./treplay [-ns] TFILE \n");
        return 1;
      
	  default:
		printf("mode not specified");
        abort ();
      }
	  filename=argv[optind];
	  stream = open(filename,2);
	  
	  
	  if(optind > 3 )
	  {
		  printf("Usage : ./treplay [-ns] TFILE \n");
		  exit(1);	
	  }
	 
	if(stream<0)
	{
		printf("Error in opening file \n");
		exit(0);
	}
	  
	
	while(1)
	{	
		retval=read(stream,buf_record_size,2);
		if(retval==0) 
		{
			break;
		}
		
		//getting record_size 
		record_size=(unsigned short)*buf_record_size;
		printf("record size : %d \n",record_size);
		record_size=record_size-2; 
		
		//getting a single record into buffer
		buffer=(char *)malloc(record_size);   
		read(stream,buffer,record_size);
		ptr=buffer; 
		
		//obtaining record_id
		record_id=(int)*ptr;
		ptr=ptr+sizeof(int);
		printf("record id is : %d \n",record_id);
		
		record_type=(int)*ptr;//record_type from buffer
		
		
		
		switch(record_type){
			case 'o':
				
				printf("record type : open \n");
				ptr=ptr+sizeof(char);
				
				open1.flags=(unsigned int)*ptr;
				printf("flags : %d \n",open1.flags);
				ptr=ptr+sizeof(unsigned int);
		
				open1.mode=(unsigned short)*ptr;
				printf("mode is: %hu \n",open1.mode);
				ptr=ptr+sizeof(unsigned short);
				
				open1.pathname_length=(unsigned short)*ptr;
				printf("path name length : %hu \n",open1.pathname_length);
				ptr=ptr+sizeof(unsigned short);
				
				open1.pathname=(char *)malloc (open1.pathname_length);
				strncpy(open1.pathname,ptr,open1.pathname_length);
				printf("path is : %s \n", open1.pathname);
				ptr=ptr+sizeof(open1.pathname_length);
				
				open1.errno=(int)*ptr;
				ptr=ptr+sizeof(int);

				if(mode==mode_default)
				{
					open1.retval=open(open1.pathname,open1.flags,open1.mode);
					printf("traced system call return value is : %d \n",open1.retval);
					printf("TRFS call return value is : %d \n ",open1.errno);
				}
				if (mode==mode_s)
				{	
					open1.retval=open(open1.pathname,open1.flags,open1.mode);
					if(open1.errno<0 && open1.retval>=0 || open1.errno>=0 && open1.retval<0)
					{
						printf("Deviation in TRFS call and traced system call \n");
						printf("traced system call return value is : %d \n",open1.retval);
						printf("TRFS call return value is : %d \n ",open1.errno);
						exit(0);
					}
					
					if(open1.errno < 0 && open1.retval<0)
					{
						if(open1.errno!=open1.retval)
						{
							printf("Deviation - return value in TRFS call : %d , return value in traced call %d",open1.errno,open1.retval );
							exit(0);
						}
						
					}
					
					printf("No deviation in traced and TRFS call \n");
					printf("traced system call return value is :%d \n",open1.retval);
				
				printf("TRFS call return value is : %d \n ",open1.errno);
				}
					
				lookup_arr[lookup_index].key=record_id;//record_is used as key for lookup 
				lookup_arr[lookup_index].fd=open1.retval;//fd to be used for corresponding read and write calls
				lookup_index++;
				
				
				break;
			
			case 'w':
				
				printf("record type : write\n");
				ptr=ptr+sizeof(char);
				
				//to lookup the corresponding open
				write1.record_id_open=(int)* ptr;
				printf("corresponding open record_id : %d \n", write1.record_id_open);
				ptr=ptr+sizeof(int);
				
				//number of bytes to be written as entered by user
				write1.count=(size_t)*ptr;
				printf("number of bytes to be written : %d \n",write1.count);
				ptr=ptr+sizeof(size_t);
				
				write1.buf=(char *)malloc(write1.count);
				strncpy(write1.buf,ptr,write1.count);
				printf("content in the write buffer : %s \n",write1.buf);
				ptr=ptr+write1.count;
				
				//return value from trfs_write
				write1.errno=(int) * ptr;
				ptr=ptr+sizeof(int);
				
				
				//to get fd of corresponding open call from lookup 
				for (i=0;i<lookup_index;i++)
				{
					if(write1.record_id_open==lookup_arr[i].key)
						write1.fd=lookup_arr[i].fd;
				}
				
				
				if(mode==mode_default)
				{
					
					if(write1.fd<0)
						printf("open before write failed \n");
					else
					{
						write1.num_bytes=write(write1.fd,write1.buf,write1.count);
						printf("traced system call return value is : %d \n",write1.num_bytes);
						printf("TRFS call return value is : %d \n ",write1.errno);
					}
				}
				
				if (mode==mode_s)
				{
					if(write1.fd<0)
					{
						printf("open before write failed \n");
						exit(0);
					}
					write1.num_bytes=write(write1.fd,write1.buf,write1.count);
					if(write1.num_bytes!=write1.count)
					{
						printf("Deviation - written bytes in TRFS call : %d , written bytes in traced call : %d \n",write1.count,write1.num_bytes);
						exit(0);
					}
					
					printf("No deviation in traced and TRFS call \n");
					printf("traced system call return value is : %d \n",write1.num_bytes);
				
				printf("TRFS call return value is :%d \n ",write1.errno);
				}
				if(write1.buf)
					free(write1.buf); 
				break;
			
			case 'r':
				
				printf("record type : read \n");
				ptr=ptr+sizeof(char);
				
				read1.record_id_open=(int)* ptr;
				printf("corresponding open record_id : %d \n",read1.record_id_open);
				ptr=ptr+sizeof(int);
				
				//bytes to be read as entered by the user
				read1.user_bytes=(size_t)*ptr;
				printf("number of bytes entered by user : %d \n",read1.user_bytes);
				ptr=ptr+sizeof(size_t);
				
				//return value from trfs_read
				read1.errno=(int)*ptr;
				printf("number of bytes read : %d \n",read1.errno);
				ptr=ptr+sizeof(int); 
				
				//to get fd of corresponding open call from lookup 
				for(i=0;i<lookup_index;i++)
				{
					if(read1.record_id_open==lookup_arr[i].key)
					{
						read1.fd=lookup_arr[i].fd;
					}
				}
				
				if(read1.errno>=0)  
				{
					read1.buf=(char *)malloc(read1.errno);
					strncpy(read1.buf,ptr,read1.errno); //content read at trfs_level
					printf("content read to buffer : %s \n ",read1.buf);
					ptr=ptr+read1.errno; 
				}
					
				if(mode==mode_default)
				{
					if(read1.fd<0)
						printf("open before read failed \n");
					else
					{
						read1.trace_buf=(char *)malloc(read1.errno); 
						read1.num_bytes=read(read1.fd,read1.trace_buf,read1.errno);
						printf("traced system call return value is : %d \n",read1.num_bytes);
					    printf("TRFS call return value is :%d \n ",read1.errno);
					}
				}
				
				if (mode==mode_s)
				{
					if(read1.fd<0)
					{
						printf("open before write failed \n");
						exit(0);
					}
					read1.trace_buf=(char *)malloc(read1.errno);
					read1.num_bytes=read(read1.fd,read1.trace_buf,read1.errno);
					if(read1.num_bytes!=read1.errno)
					{
						printf("Deviation - read bytes in TRFS call : %d , read bytes in traced call : %d \n",read1.errno,read1.num_bytes);
						exit(0);
					}
					if(read1.buf){
						if(strncmp(read1.trace_buf,read1.buf,read1.num_bytes)!=0)
						{
							printf("Deviation - read content in TRFS call : %s , read content in traced call : %s \n",read1.buf,read1.trace_buf);
							exit(0);
						}
					}
					printf("No deviation in traced and TRFS call \n");
					printf("traced system call return value is : %d \n",read1.num_bytes);
				
				printf("TRFS call return value is : %d \n ",read1.errno);
				}
		
				if(read1.buf)
					free(read1.buf);
				if(read1.trace_buf)
					free(read1.trace_buf);
				
				break;
				
			case 'c':
				printf("record type : close \n");
				ptr=ptr+sizeof(char);
				
				close1.record_id_open=(int)* ptr;
				printf("corresponding open record_id : %d \n",close1.record_id_open);
				ptr=ptr+sizeof(int);
				
				//lookup for corresponding open
				for(i=0;i<lookup_index;i++)
				{
					if(close1.record_id_open==lookup_arr[i].key)
					{
						close1.fd=lookup_arr[i].fd;
					}
				}
				
				if(mode==mode_default)
				{
					if(close1.fd<0)
						printf("open before write failed \n");
					else
					{
						close1.retval=close(close1.fd);
						printf("traced system call return value is : %d \n",close1.retval);
					}
				}
				if(mode==mode_s)
				{
					
					if(close1.fd<0)
					{
						printf("open before close failed \n");
						exit(0);
					}
					else
					{
						close1.retval=close(close1.fd);
						printf("traced system call return value is : %d \n",close1.retval);
					}
					
				}
				break;
				
			case 'm':
				
				printf("record type : Make Directory \n");
				ptr=ptr+sizeof(char);
				
				mkdir1.mode=(unsigned short)*ptr;
				printf("mode of cretaing directory %hu \n",mkdir1.mode);
				ptr=ptr+sizeof(unsigned short);
				
				mkdir1.path_size=(unsigned short)*ptr;
				printf("Path size is : %hu \n",mkdir1.path_size);
				ptr=ptr+sizeof(unsigned short);
				
				mkdir1.path=(char *)malloc(sizeof(mkdir1.path_size));
				strncpy(mkdir1.path,ptr,mkdir1.path_size);
				printf("path name for mkdir : %s\n",mkdir1.path);
				ptr=ptr+sizeof(mkdir1.path_size);
					
				mkdir1.errno=(int)*ptr;
				ptr=ptr+sizeof(int);
				
				if(mode==mode_default)
				{
					
					mkdir1.retval=mkdir(mkdir1.path,mkdir1.mode);
					
					
					printf("traced system call return value is : %d \n",mkdir1.retval);
					printf("TRFS call return value is : %d \n ",mkdir1.errno);
				
				}
				if(mode==mode_s)
				{
					mkdir1.retval=mkdir(mkdir1.path,mkdir1.mode);
					if(mkdir1.retval<=0 &&  mkdir1.errno<=0)
					{
						if(mkdir1.retval!=mkdir1.errno)
						{
						printf("Deviation - return value in TRFS call : %d,return value in traced call %d \n",mkdir1.errno,mkdir1.retval);
						exit(0);
						}
					}
					
					
				}
				break;
				
			case 'R':
				printf("Record type : Remove Directory \n");
				ptr=ptr+sizeof(char);
				
				rmdir1.path_size=(unsigned short)*ptr;
				printf("size of rmdir path : %d \n",rmdir1.path_size);
				ptr=ptr+sizeof(unsigned short);
				
				rmdir1.path=(char *)malloc(rmdir1.path_size);
				strncpy(rmdir1.path,ptr,rmdir1.path_size);
				printf("path name for rmdir : %s \n",rmdir1.path);
				ptr=ptr+sizeof(rmdir1.path_size);
				
				rmdir1.errno=(int)*ptr;
				ptr=ptr+sizeof(rmdir1.errno);
				
				if(mode==mode_default)
				{
					
					rmdir1.retval=rmdir(rmdir1.path);
					
					
					printf("traced system call return value is  : %d \n",rmdir1.retval);
					printf("TRFS call return value is : %d \n ",rmdir1.errno);
				
				}
				if(mode==mode_s)
				{
					rmdir1.retval=rmdir(rmdir1.path);
					if(rmdir1.retval<=0 &&  rmdir1.errno<=0)
					{
						if(rmdir1.retval!=rmdir1.errno)
						
						printf("Deviation -return value in TRFS call : %d,return value in traced call : %d \n",rmdir1.errno,rmdir1.retval);
					}
					
					
				}
				break;
				
				
				
				
				
				
				
		}
		
		
		
	 printf("\n");	
	 if(buffer)
	 free(buffer); 
	}
	close(stream);
	  return 0;
}
