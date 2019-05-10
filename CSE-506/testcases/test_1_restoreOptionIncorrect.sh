#!/bin/sh

#THIS TEST CASE CHECKS FOR INCORRECT OPTION.

gcc -Wall xhw2.c -o xhw2 #USER LEVEL
gcc -Wall createfile_1.c -o createfile_1
gcc -Wall createfile_2.c -o createfile_2
./createfile_1 
./createfile_2
./xhw2 /mnt/bkpfs/newfile.txt -r random #Incorrect Option
retval=$?
if [ $retval != 0 ] ; then
	echo "[INFO] SUCCESSFULL"
else
	echo "[INFO] COULD NOT RUN"
	exit 1
fi