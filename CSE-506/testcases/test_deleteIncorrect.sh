#!/bin/sh

#THIS TEST CASE TRIES DELETING INCORRECT BACKUP VERSION.
#EXPECED BEHAVIOUR FAILURE


gcc -Wall xhw2.c -o xhw2 #USER LEVEL
gcc -Wall createfile_1.c -o createfile_1

./createfile_1 
echo "[INFO] LIST FILES"
./xhw2 /mnt/bkpfs/newfile.txt -l
./xhw2 /mnt/bkpfs/newfile.txt -d 0 
retval=$?
if [ $retval != 0 ] ; then
	echo "[INFO] FILES REMAINING"
	./xhw2 /mnt/bkpfs/newfile.txt -l
	echo "[INFO] SUCCESSFULL"
else
	echo "[INFO] COULD NOT RUN."
	exit 1
fi
