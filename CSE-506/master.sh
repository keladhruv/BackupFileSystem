#!/bin/sh

#THIS IS THE MASTER TEST SCRIPT WHICH CALLS THE OTHER TEST SCRIPTS

rm -rf /delete/out/*
rm -rf /mnt/bkpfs/*
mkdir -p /delete/out/
mkdir -p /mnt/bkpfs/
cp xhw2.c testcases/.
chmod 777 testcases/*

for file in testcases/test*; do
	./${file} 
	retval=$?
	if [ $retval != 0 ] ; then
		echo "[MASTER TEST SCRIPT:FAILURE]TEST SCRIPT : ${file} FAILED"
	else
		echo "[MASTER TEST SCRIPT:SUCCESS]TEST SCRIPT : ${file} PASSED"
	fi
	done

rm -rf createfile_1
rm -rf createfile_2
rm -rf testcases/xhw2.c
rm -rf xhw2
