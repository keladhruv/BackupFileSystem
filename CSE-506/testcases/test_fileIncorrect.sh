#!/bin/sh

#THIS TEST CASE TRIES GIVING AN INCORRECT FILE AS INPUT.
#EXPECTED BEHAVIOUR FAILIURE


gcc -Wall xhw2.c -o xhw2 #USER LEVEL
gcc -Wall createfile_1.c -o createfile_1

./createfile_1 
./xhw2 /mnt/bkpfs/newfil -l
retval=$?
if [ $retval != 0 ] ; then
	echo "[INFO] FILES REMAINING"
	./xhw2 /mnt/bkpfs/newfile.txt -l
	echo "[INFO] SUCCESSFULL"
else
	echo "[INFO] COULD NOT RUN."
	exit 1
fi
