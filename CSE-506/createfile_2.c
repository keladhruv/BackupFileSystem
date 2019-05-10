/*
 C program to create a file and write data into file.
 
*/

#include <asm/unistd.h>
#include </usr/include/stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{ 	
	int fd;
	fd = open("/mnt/bkpfs/newfile.txt", O_RDWR|O_CREAT,S_IRWXU | S_IRWXG | S_IRWXO);
	write(fd, "Operating Systems Course SPRING 2019\n", 36);
	close(fd);
    return 0;
}