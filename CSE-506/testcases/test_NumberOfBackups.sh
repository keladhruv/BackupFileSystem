#!/bin/sh

#THIS TEST CASE CHECKS THAT THERE ARE ALWAYS ONLY 5 Backups at any given period of time.


gcc -Wall xhw2.c -o xhw2 #USER LEVEL
gcc -Wall createfile_1.c -o createfile_1

./createfile_1 
./xhw2 /mnt/bkpfs/newfile.txt -l
retval=$?
if [ $retval == 0 ] ; then
	echo "[INFO] SUCCESSFULL"
else
	echo "[INFO] COULD NOT RUN."
	exit 1
fi
