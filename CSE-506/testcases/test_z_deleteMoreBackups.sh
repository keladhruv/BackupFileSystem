#!/bin/sh

#THIS TEST CASE TRIES DELETING MORE BACKUPS THAN AVAILABLE.
#EXPECTED BEHAVIOUR FAILIURE

gcc -Wall xhw2.c -o xhw2 #USER LEVEL
gcc -Wall createfile_1.c -o createfile_1

./createfile_1 
echo "[INFO] LIST FILES"
./xhw2 /mnt/bkpfs/newfile.txt -l
./xhw2 /mnt/bkpfs/newfile.txt -d oldest 
./xhw2 /mnt/bkpfs/newfile.txt -d oldest 
./xhw2 /mnt/bkpfs/newfile.txt -d oldest 
./xhw2 /mnt/bkpfs/newfile.txt -d oldest 
./xhw2 /mnt/bkpfs/newfile.txt -d oldest 
./xhw2 /mnt/bkpfs/newfile.txt -d oldest 
./xhw2 /mnt/bkpfs/newfile.txt -d oldest 
./xhw2 /mnt/bkpfs/newfile.txt -d oldest 
retval=$?
if [ $retval != 0 ] ; then
	echo "[INFO] SUCCESSFULL"
else
	echo "[INFO] COULD NOT RUN."
	exit 1
fi
