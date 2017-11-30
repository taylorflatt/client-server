#!/bin/bash

# Version 0.2
# Author: Norman Carver, Taylor Flatt
# Script which will join n-batches of 100 clients to the server and run a string of commands 
# m-times (m cycles).
#
# If the script hangs and no output is displayed, then likely all of the clients aren't finished 
# running and/or not all have been accepted/run and are ghosting in the client. Investigate with 
# running other auxiliary scripts such as show-server/show-clients/show-sockets.
#
# Note: The script also calls the client-no-tty client since running headless, we don't want to 
# make modifications to the tty.
#
# Usage: floodServer NUMCLIENTS

function print_usage()
{
	echo -e "\nUsage: $0 -n NUMCLIENTS [-l]\n"
}

# Displays the help information to a user.
function print_help()
{
	print_usage
	echo -e "Joins n-clients to a server and waits until all children are collected to complete. \n"
	
	echo -e "\t -n NUMCLIENTS \t The number of clients who will be created."
    echo -e "\t -l \t\t Enables line rewriting so the terminal isn't saturated with the client creation information."
    echo -e "\t -h \t\t Prints this help message. \n"
	
	echo "Note:"
	echo -e "\t-It may take some time for the script to finish executing since it waits for all of the clients to exit prior to returning. However, if it doesn't exit, then there is a problem with the server.\n"
	
	echo "Example:"
	echo -e "\t./flood_server_test -n 1000 \n"
	echo -e "\tWill create 1000 good clients and connect them to a server rapidly.\n"
	
	echo -e "Full documentation and source code can be found at: <www.github.com/taylorflatt/client-server>.\n"
}

# Font colors for error/success messages.
RED=`tput setaf 1`
GREEN=`tput setaf 2`
END_COLOR=`tput sgr0`

# Sequence of commands to be sent to server:
# (Separate each command line with \n, exit automatically sent at end.)
clientcommands=$'pwd\nexit'

if [[ $# < 1 || $# > 4 ]]; then
    print_usage
    exit 1
fi

scriptdir=$(dirname "$0")
linewriting=0

# Parse the arguments.
while getopts ":n:lh" opt; do
	case $opt in
    n)
		nclients=$OPTARG
		;;
	l)
		linewriting=1
		;;
    h)
		print_help
        exit 0
		;;
	?)
		print_usage
		exit 1	
		;;
	esac
done

if [[ -z ${nclients+x} ]]; then
    echo -e "\nError: Please specify the number of clients!"
    print_usage
    exit 1
fi

function clientscript()
{
    # Change the separator from spaces to a newline 
    # so we can send commands that include spaces.
    IFS=$'\n'
    echo "unset HISTFILE"
    for d in $(seq 1 "$nclients"); do
        for cmd in $clientcommands; do
            echo $cmd
            sleep 3
        done
    done

    echo "exit"

    #Keep stdin open briefly:
    sleep 2

    exit
}

function remove_error_file() {

    if [[ -e testerrors ]]; then
        if ! rm -f testerrors; then
            echo "Failed to remove any previous testerrors files! Manually remove the file to continue."
            return -1
        fi
    fi

    return 0
}

for ((i=1; i<=2; i++)); do
    if [[ ! -x client-no-tty-tester ]]; then

        # If this is the second time checking for the broken clients, then just error out.
        if [[ i -eq 2 ]]; then
            echo "${RED}Error: Already tried making client-no-tty-tester and it still doesn't exist!${END_COLOR}"
            exit 1
        fi

        if [[ ! -e makefile ]]; then
            echo "${RED}Error: client-no-tty-tester doesn't exist and there isn't a makefile to try and make it.${END_COLOR}"
            exit 1
        fi

        echo "Attempting to make the client..."

        if ! make nottyclient; then
            echo "${RED}Error: Failed making broken clients.${END_COLOR}"
            exit 1
        fi
    fi
done

if lsof -i :4070 &> /dev/null; then
    echo "Server is running!"
else
    echo "Error: server does not seem to be running"
    exit 1
fi

remove_error_file

echo -e "\nTest Parameters:"
echo "--------------------------------"
echo "Number of clients = $nclients"
echo "--------------------------------"

echo -e "\nBeginning the server flood...\n";

SECONDS=0
# Run specified number of clients against server and drop client output.:
for (( i=1; i<="$nclients"; i++ )); do
    
    clientscript | "$scriptdir"/client-no-tty-tester 127.0.0.1 &> /dev/null &
    cpid=${!}

    echo "($i/$nclients) Adding client ($cpid) and running commands..."
    if [[ $linewriting -eq 1 ]]; then
        tput civis      # Set the cursor to invisible momentarily.
        tput cuu1       # Move the cursor up one line.
        tput el         # Delete the entire line in the terminal.
        tput cnorm      # Reset the cursor visibility.
    fi
done

echo -e "\nWaiting for clients to exit..."

# Waits for all children to close prior to exiting. If the 
# script hangs, then all children aren't finishing/exiting.
# That would be an error.
wait

echo -e "\nDone Testing!\n"

echo -e "\nTest Results:"
echo "--------------------------------"
echo "Number of clients = $nclients"
if (($SECONDS > 60)); then
    min=($SECONDS%%3600)/60
    sec=($SECONDS%%3600)%60
    echo "Elapsed time = $min:$sec minutes"
else
    echo "Elapsed time = $SECONDS seconds"
fi
echo "--------------------------------"

# Some errors may be output during the kill/killall but these are fine. So long as no errors 
# were encountered during the runtime of flooding the server with clients, things are working 
# fine.
if [[ -s testerrors ]]; then
    echo -e "${RED}Error messages generated, see file: testerrors${END_COLOR}\n"
    exit 1
else
    echo -e "${GREEN}Successfully passed all testing with $nclients clients!${END_COLOR}\n"
    remove_error_file
    exit 0
fi

#EOF
