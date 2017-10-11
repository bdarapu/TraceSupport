/*
  User program which sets and removes system call vectors through ioctl to this user program's process
 */

#include "ioctl.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_IOCTL_PATH_LENGTH 512

/*Functions to remove and set ioctl vectors*/

int ioctl_remove_vector(int fd, struct param *ioctl_param)
{
  int ret_value = 0;
  ret_value = ioctl(fd, REMOVE_IOCTL, ioctl_param);
  if (ret_value < 0) {
    printf("ioctl_remove_vector failed:%d %d\n", errno, fd);
    perror("ERROR ");
  }
  return ret_value;
}

int ioctl_set_vector(int fd, struct param *ioctl_param)
{
  int ret_value = 0;

  ret_value = ioctl(fd, SET_IOCTL, ioctl_param);
  if (ret_value < 0) {
    printf("ioctl_set_vector failed:%d %d %d \n", errno, fd,
           ret_value);
    perror("ERROR ");
  }

  return ret_value;
}

/** Main program - ioctl functions called from here
 */
int main(int argc, char **argv)
{
  int child_id = -1;
  int fd;
  struct param *ioctl_param = NULL;
  int ret_value = 0;
  char *dummy_file_ioctl;
  char device[] = "/dev/ioctl_device";
  int vec_id;
  int list_pid;

  if ((argc < 3) || ((((int)*argv[1]) - '0') >= 6) || (argc > 3)) {
    printf
        ("Syntax to run: \n$/> ./pass_ioctl {vector_id} and vector-id range in [0,5]\n");
    goto main_out;
  }

  printf("My process ID : %d\n", getpid());

  dummy_file_ioctl = (char *)malloc(MAX_IOCTL_PATH_LENGTH);
  memset(dummy_file_ioctl, 0, MAX_IOCTL_PATH_LENGTH);
  memcpy(dummy_file_ioctl, device, strlen(device));

  ioctl_param = (struct param *)malloc(sizeof(struct param));
  ioctl_param->v_id = (int)*argv[1];
  ioctl_param->v_id = ioctl_param->v_id - '0';

  printf("Vector id passed : %d\n", ioctl_param->v_id);
  fd = open(dummy_file_ioctl, 0);

  if (fd < 0) {
    printf("Can't open file: %s\n", dummy_file_ioctl);
    goto out1;
  }

  ioctl_param->p_id = getpid();

  ret_value = ioctl_set_vector(fd, ioctl_param);
  if (ret_value < 0) {
    printf("error\n");
    goto out1;
  }

  int vector_id;
  int read_value, stat_val, unlink_val;
  size_t count;
  char *buff;
  struct stat metadata;
  printf("Calling wrapped open syscall \n");
  ret_value = open("test_open", O_CREAT, 777);
  ret_value = open("test_open1", O_CREAT, 777);
  ret_value = open("test_open2", O_WRONLY, 777);

  buff = malloc(sizeof(char) * 100);
  printf("Calling overridden read syscall \n");
  read_value = read(ret_value, buff, 100);
  printf("Buffer read from file is %s\n", buff);
  printf("Calling wrap write syscall \n");
  read_value = write(ret_value, "OPERATING SYSTEMS HOMEWORK3", 27);
  printf("Calling stat for protected files \n");
  stat_val = stat("test_open2", &metadata);

  if (stat_val == 0) {
    printf("File Size: \t\t %d bytes\n", metadata.st_size);
  } else {
    printf("Cant get stat of a protected file \n");
  }

  unlink_val = unlink("test_open");
  if (unlink_val == 0) {
    printf(" unlink successful \n");
  } else {
    printf(" Cant unlink for a protected file \n");
  }
  sleep(40);
  ret_value = ioctl_remove_vector(fd, ioctl_param);
  ioctl_param->v_id = (int)*argv[2];
  ioctl_param->v_id = ioctl_param->v_id - '0';
  ret_value = ioctl_set_vector(fd, ioctl_param);

  sleep(40);
  mkdir("mkdir_test", 777);
  chdir("chdir_test");
  rmdir("rmdir_test");
  int pid = getpid();
  // ret_value = ioctl(fd, LIST_ID, &pid);
  printf(" vector id is %d\n", pid);
  // ioctl_param->v_id = pid;
  ret_value = ioctl_remove_vector(fd, ioctl_param);
  if (ret_value < 0) {
    goto out1;
  }
  close(fd);
  free(buff);
out1:
  if (dummy_file_ioctl) {
    free(dummy_file_ioctl);
  }
  if (ioctl_param) {
    free(ioctl_param);
  }

main_out:
  return ret_value;
}
