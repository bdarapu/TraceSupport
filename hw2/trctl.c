#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "trctl.h"


int main(int argc , char * argv[])
{
	
	int fd;
	char mount_point[256];
	
	int ret,i;
	char * validate_hex;
	unsigned long x=0;
	if(argc!=2 && argc!=3)
	{
		printf("Error : Usage is ./trctl cmd /mounted/path \n or ./trctl /mounted/path");
		exit(1);
	}
	if(argc==3){
		
		strcpy(mount_point, argv[2]);
		//fd = open(mount_point,O_RDONLY);
		strcpy(validate_hex,argv[1]);
		
		
		
		if(strcmp(argv[1],"all")==0)
		{	
			x=2147483647;
			fd = open(mount_point,O_RDONLY);
				if(fd <0 ) 
			{                                 
				printf(" failed to open %s \n",mount_point);  
				exit (1);                                     
			}
			ret=ioctl(fd,BITMAP_ALL_VALUE,&x);
		}
		else if(strcmp(argv[1],"none")==0)
		{	
			x=0;
			fd = open(mount_point,O_RDONLY);
				if(fd <0 ) 
			{                                 
				printf(" failed to open %s \n",mount_point);  
				exit (1);                                     
			}
			
			ret=ioctl(fd,BITMAP_NONE_VALUE,&x);
		}
		
		else if(strcmp(argv[1],"all")!=0 && strcmp(argv[1],"none")!=0)
		{
			if(validate_hex[0]!='0' || validate_hex[1]!='x' || (validate_hex[strspn(validate_hex, "0123456789xabcdefABCDEF")] != 0))
				printf("Error : Usage is ./trctl cmd /mounted/path \n or ./trctl /mounted/path : Acceptable values for cmd -<all> <none> <0xab> \n");
			else 
			{	
				x=strtol(argv[1], NULL, 16);
				fd = open(mount_point,O_RDONLY);
				if(fd <0 ) 
			{                                 
				printf(" failed to open %s \n",mount_point);  
				exit (1);                                     
			}
				ret=ioctl(fd,BITMAP_HEX_VALUE,&x);
			}
			
		}
		
		
	}
		
	else if(argc==2){
		
		strcpy(mount_point, argv[1]);
		fd=open(mount_point,O_RDONLY);
		if(fd<0)
		{
			printf(" failed to open %s \n",mount_point);  
			exit (1);                                  
		}
		
		
		ret=ioctl(fd,BITMAP_GET_VALUE,&x);
		
		printf("current value of bitmap set to  : 0x%x \n",x);
		
	}
		
   	                         
        close(fd);                                      
        return 0;                                          
	             
} 
