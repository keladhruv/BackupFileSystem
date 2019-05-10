#!/bin/sh

#THIS TEST CASE CHECK IF MULTIPLE BACKUP FILES ARE SUCCESSFULLY CREATED IN THE MOUNT PATH WITH DIFFERENT DATA.

gcc -Wall xhw2.c -o xhw2 #USER LEVEL
gcc -Wall createfile_1.c -o createfile_1
gcc -Wall createfile_1.c -o createfile_2

./createfile_1 #Differrent Data from createfile_2
./createfile_2
./createfile_1
./createfile_2
./xhw2 /mnt/bkpfs/newfile.txt -l
retval=$?
if [ $retval == 0 ] ; then
	echo "[CREATE FILE:INFO] SUCCESSFULL"
else
	echo "[CREATE FILE:INFO] COULD NOT RUN"
	exit 1
fi
