#!/bin/bash

#while true; do
#	cat client.c
#done

counter=0

while [ $counter -gt -1 ]; do
	echo $counter
	((counter++))
done

exit

