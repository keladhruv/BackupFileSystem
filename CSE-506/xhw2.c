#include <asm/unistd.h>
#include </usr/include/stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include<sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#define delete_bkp _IOW('a', 'a', int*)
#define list _IOR('a', 'b', int*)
#define list_2 _IOR('a', 'c', int*)
#define restore _IOW('a', 'd', int*)
#define view _IOW('a', 'e', int*)
#define view_2 _IOWR('a', 'i', char*)
#define PAGE_SIZE 4096
/*
	Checks if filepath is NULL and for user permissions.
*/
void fileValidity(char *infile)
{
	struct stat statRes;
	int exist;
	if (infile == NULL) {
		fprintf(stderr,  "[ERROR] Please mention the Input File. \n");
		exit(EXIT_FAILURE);
	}
    exist = stat(infile, &statRes);
    if (exist != 0) {
		fprintf(stderr, "[ERROR] Invalid File \n");
		exit(EXIT_FAILURE);
	}
}
/*
	Checks Input Validity.
*/
void inputValidity(char *input, int flag_l, int flag_d, int flag_r, int flag_v)
{
	if (flag_d) {
		if (strcmp(input, "newest") != 0 && strcmp(input, "oldest") != 0 && strcmp(input, "all") != 0) {
			fprintf(stderr,  "[ERROR] Please choose one of the following arguments \"newest\" | \"oldest\" | \"all\" \n");
			exit(EXIT_FAILURE);
		}
	}
	if (flag_v) {
		if (strcmp(input, "newest") != 0 && strcmp(input, "oldest") != 0) {
			int len = strlen(input);
			int i = 0;
			int isDigit = 1;
			for (i = 0; i < len; i++) {
				if (input[i] > '9' || input[i] < '0') {
					isDigit = 0;
					break;
				}
			}
			if (isDigit == 0) {
				fprintf(stderr,  "[ERROR] Please choose one of the following arguments \"newest\" | \"oldest\" | \"N\" where N is file Backup number \n");
				exit(EXIT_FAILURE);
			}
		}
	}
	if (flag_r) {
		if (strcmp(input, "newest") != 0 && strcmp(input, "oldest") != 0) {
			int len = strlen(input);
			int i = 0;
			int isDigit = 1;
			for (i = 0; i < len; i++) {
				if (input[i] > '9' || input[i] < '0') {
					isDigit = 0;
					break;
				}
			}
			if (isDigit == 0) {
				fprintf(stderr,  "[ERROR] Please choose one of the following arguments \"newest\"|\"oldest\"|\"N\" where N is file Backup number \n");
				exit(EXIT_FAILURE);
			}
		}
	}
}
/*
	Checks if at least one flag is given.
*/
void flagValidity(int flag_l, int flag_d, int flag_r, int flag_v)
{
	if (flag_l == 0 && flag_r == 0 && flag_d == 0 && flag_v == 0) {
		fprintf(stderr,  "[ERROR] Please choose one of the following {-l|-d|-v|-r} \n");
		exit(EXIT_FAILURE);
	}
}
int main(int argc, char * const argv[])
{
	/* Variable Decleration */
	int rc = 0;
	char *input, *infile;
	char *buf = malloc(PAGE_SIZE*sizeof(char));
	int flag_l = 0, flag_d = 0, flag_r = 0, flag_v = 0;
	int c = 0, fd, old, new;
	int count = 0;
	char name[255];
	char num_count[100];
	int value, version;

	while ((c = getopt(argc,  argv, "ld:r:v:")) != -1) {

		switch (c) {
		case 'd':
				if (flag_l == 1 || flag_v == 1 || flag_r == 1) {
					fprintf(stderr, "[ERROR] Please chose one of {-l|-d|-r|-v}. \n");
				} else if (flag_d == 0) {
					input = optarg;
					flag_d = 1;
				} else {
					fprintf(stderr,  "[ERROR] %s: -d cannot be used more than once. \n",  argv[0]);
					exit(EXIT_FAILURE);
				}
				break;
		case ':':
				fprintf(stderr,  "[ERROR] %s: -d  option should be followed by {newest|oldest|all}.\n",  argv[0]);
				exit(EXIT_FAILURE);
				break;
		case 'v':
				if (flag_l == 1 || flag_d == 1 || flag_r == 1) {
					fprintf(stderr, "[ERROR] Please chose one of {-l|-d|-r|-v}. \n");
				} else if (flag_v == 0) {
					input = optarg;
					flag_v = 1;
				} else {
					fprintf(stderr,  "[ERROR] %s: -v cannot be used more than once. \n",  argv[0]);
					exit(EXIT_FAILURE);
				}
				break;
		case 'r':
				if (flag_l == 1 || flag_v == 1 || flag_r == 1) {
					fprintf(stderr, "[ERROR] Please chose one of {-l|-d|-r|-v}. \n");
				} else if (flag_r == 0) {
					input = optarg;
					flag_r = 1;
				} else {
					fprintf(stderr,  "[ERROR] %s: -r cannot be used more than once. \n",  argv[0]);
					exit(EXIT_FAILURE);
				}
				break;
		case 'l':
				if (flag_d == 1 || flag_v == 1 || flag_r == 1) {
					fprintf(stderr, "[ERROR] Please chose one of {-l|-d|-r|-v}. \n");
					exit(EXIT_FAILURE);
				} else if (flag_l == 1) {
					fprintf(stderr, "[ERROR] -l cannot be used more than once. \n");
					exit(EXIT_FAILURE);
				}
				flag_l = 1;
				break;
		case 'h':
				fprintf(stdout, "Usage: %s {-l | -d delete_option | -v view_option | -r restore_option} [-h HELP] inputFile \n", argv[0]);
				fprintf(stdout, "-l : List all verstions\n");
				fprintf(stdout, "-d : Delete File Version\n");
				fprintf(stdout, "-v : View File Version \n");
				fprintf(stdout, "-r : Restore File Version \n");
				fprintf(stdout, "-h : Help \n");
				fprintf(stdout, "inputFile: Input File\n");
				fprintf(stdout, "delete_option: \"newest\" | \"oldest\" | \"all\"\n");
				fprintf(stdout, "view_option: \"newest\" | \"oldest\" | \"N (Where N is version number)\"\n");
				fprintf(stdout, "restore_option: \"newest\" | \"N (Where N is version number)\"\n");
				exit(EXIT_FAILURE);
				break;
		case '?':
				fprintf(stderr,  "[ERROR] Please check %s -h\n",  argv[0]);
				exit(EXIT_FAILURE);
				break;
		}
	}
	infile = argv[optind];
	flagValidity(flag_l, flag_d, flag_r, flag_v);
	inputValidity(input, flag_l, flag_d, flag_r, flag_v);
	fileValidity(infile);
	fd = open(infile, O_RDWR|O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
	/*IOCTL CALLS*/
	if (flag_l) {
		ioctl(fd, list, (int *) &new);
		ioctl(fd, list_2, (int *) &old);
		if (old > new) {
			printf("No Backups available\n");
		}
		printf("The following versions are available-\n");
		for (count = old; count <= new; count++) {
			strcpy(name, ".");
			strcat(name, infile);
			strcat(name, ".bkp.");
			sprintf(num_count, "%d", count);
			strcat(name, num_count);
			printf("%s\n", name);
		}
	}
	if (flag_r) {
		if (strcmp(input, "newest") == 0) {
			printf("Restoring newest.\n");
			version = -1;
		} else if (strcmp(input, "oldest") == 0) {
			printf("Restoring oldest.\n");
			version = -2;
		} else {
			ioctl(fd, list, (int *) &new);
			ioctl(fd, list_2, (int *) &old);
			value = atoi(input);
			if (value > new || value < old) {
				printf("Version %s does not exist.\n", input);
				exit(EXIT_FAILURE);
			} else {
				version = value;
				printf("Restoring Version %d.\n", version);
			}
		}
		ioctl(fd, restore, (int *) &version);
	}
	if (flag_d) {
		ioctl(fd, list, (int *) &new);
		ioctl(fd, list_2, (int *) &old);
		value = atoi(input);
		if (new < old) {
			printf("No Files to delete.\n");
			exit(EXIT_FAILURE);
		}
		if (strcmp(input, "newest") == 0) {
			printf("Deleting newest.\n");
			version = -1;
		} else if (strcmp(input, "oldest") == 0) {
			printf ("Deleting oldest.\n");
			version = -2;
		} else if (strcmp(input, "all") == 0) {
			printf ("Deleting all.\n");
			version = -3;
		}
		ioctl (fd, delete_bkp, (int *) &version);
	}
	if (flag_v) {
		if (strcmp(input, "newest") == 0) {
			version = -1;
		} else if (strcmp(input, "oldest") == 0) {
			version = -2;
		} else {
			ioctl(fd, list, (int *) &new);
			ioctl(fd, list_2, (int *) &old);
			value = atoi(input);
			if (value > new || value < old) {
				printf("Version %s does not exist.\n", input);
			} else {
				version = value;
			}
			exit(EXIT_FAILURE);
		}
		ioctl(fd, view, (int *) &version);
		ioctl(fd, view_2, buf);
		printf("%s\n", buf);
	}
	close(fd);
	free(buf);
	exit(rc);
}
