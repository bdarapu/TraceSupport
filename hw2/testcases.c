#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* Trace replay program */
int main()
{
	char *buf = NULL;
	char *buf2 = NULL;
	int err = 0;
	int fd1, fd2, fd3;

	fd1 = open("upper/a.txt", O_RDONLY, 0);
	if(fd1 == -1){
		perror("unable to open the fd");
		goto out;
	}
	else{
		printf(" fd opened is %d\n", fd1);
	}
	
	buf = (char *)calloc(PAGE_SIZE, sizeof(char));
	
	if(buf==NULL){
		perror("Not able to allocate memory\n");
		goto out;
	}

	err = read(fd1, buf, 12);
	if(err < 0){
		perror("Unable to Read bytes");
		goto out;
	}
	
	printf("Read from file, buf:%s\n", buf);	
	
	fd2 = open("upper/b.txt", O_RDONLY, 0);
	if(fd2 == -1){
		perror("unable to open fd2");
		goto out;
	}
	else{
		printf("New fd opened is %d\n", fd2);
	}
	fd3 = open("upper/c.txt", O_WRONLY, 0);
	if(fd3 == -1){
		perror("unable to open fd");
		goto out;
	}
	else{
		printf("New fd opened is %d\n", fd3);
	}
	
	//copying from file fd2 to fd3
	buf2 = malloc(1000);
	err = read(fd2, buf2, 1000);
	if(err < 0){
		perror("Unable to Read");
		goto out;
	}
	
	err = write(fd3, buf2, err);
	if(err < 0){
		perror("Unable to Write");
		goto out;
	}
	
	
	if (mkdir("upper/testing",0777) == -1) {
        	perror("Failed to Make a directory");
        	goto out;
   	 }
	
	if (mkdir("upper/tempdir",0777) == -1) {
                perror("Failed to Make a directory");
		goto out;
	}
		
	if(rmdir("upper/tempdir") == -1){
		perror("failed to delete the directory");
		goto out;
	}

	

out:
	if(buf)
		free(buf);
	if(buf2)
		free(buf2);
	
	close(fd1);
	close(fd2);
	close(fd3);
	return 0;
}
