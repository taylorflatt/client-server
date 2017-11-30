#!/bin/bash

# Version 0.1
# Author: Norman Carver, Taylor Flatt
# Script which will connect g-good_clients and b-bad_clients to a server and wait until all children 
# are collected to complete.
#
# You must specify at least -g or -b (the script cannot have no arguments). That means you can run 
# ALL good or ALL broken clients without mixing them.
#
# If the script hangs and no output is displayed, then likely all of the clients aren't finished 
# running and/or not all have been accepted/run and are ghosting in the client. Investigate with 
# running other auxiliary scripts such as show-server/show-clients/show-sockets.
#
#
# Usage: floodServer -g NUM_GOOD_CLIENTS -b NUM_BAD_CLIENTS [-l]

function print_usage()
{
	echo -e "\nUsage: $0 -g NUM_GOOD_CLIENTS -b NUM_BAD_CLIENTS [-l]\n"
}

# Displays the help information to a user.
function print_help()
{
	print_usage
	echo -e "Joins g-good_clients and b-bad_clients to a server and waits until all children are collected to complete. \n"
	
	echo -e "\t -g NUM_GOOD_CLIENTS \t The number of good clients who will be created."
	echo -e "\t -b NUM_BAD_CLIENTS \t\t The number of bad clients who will be created."
    echo -e "\t -l \t\t\t Enables line rewriting so the terminal isn't saturated with the client creation information."
    echo -e "\t -h \t\t\t Prints this help message. \n"
	
	echo "Note:"
	echo -e "\t-It may take some time for the script to finish executing since it waits for all of the clients to exit prior to returning. However, if it doesn't exit, then there is a problem with the server.\n"
	
	echo "Example:"
	echo -e "\t./flood_server_hybrid_test -g 100 -b 50 \n"
	echo -e "\tWill create 100 good clients and 50 bad clients totalling 150 clients connecting them to a server rapidly.\n"
	
	echo -e "Full documentation and source code can be found at: <www.github.com/taylorflatt/client-server>.\n"
}

# Font colors for error/success messages.
RED=`tput setaf 1`
GREEN=`tput setaf 2`
END_COLOR=`tput sgr0`

# Check arguments.
if [[ $# < 2 || $# > 6 ]]; then
    print_usage
    exit 1
fi

clientcommands=$'pwd\nexit'         # Sequence of commands to be sent to server.
scriptdir=$(dirname "$0")           # Get the directory the script is located in.
linewriting=0                       # Flag as to whether single line writing is on.
nbclientsrun=0                      # Number of bad clients that have been created.
ngclientsrun=0                      # Number of good clients that have been created.                     

# Create an array to hold all of the broken client file names.
declare -a client_list
client_list+=("client-failed-challenge")
client_list+=("client-failed-port")
client_list+=("client-failed-proceed")
client_list+=("client-failed-secret")
client_list+=("client-parent-sigint")
client_list+=("client-parent-sigquit")
client_list+=("client-wait-on-connect")
client_list+=("client-wait-on-handshake")

# Parse the arguments.
while getopts ":g::b:lh" opt; do
	case $opt in
    g)
		ngclients=$OPTARG       # Number of good clients.
		;;
    b)
		nbclients=$OPTARG       # Number of bad clients.
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

nclients=$((ngclients + nbclients)) # Number of total clients.
bclientsleft=$nbclients             # Number of bad clients yet to be created.

# Make sure that the number of good and bad clients are set. This uses variable expansion 
# which will overwrite the variable ngclients if it is set, otherwise it evaluates to nothingness
# if ngclients is not set.
if [[ -z ${ngclients+x} ]]; then
    echo -e "\nError: Please specify the number of good clients!"
    print_usage
    exit 1
fi

if [[ -z ${nbclients+x} ]]; then
    echo -e "\nError: Please specify the number of bad clients!"
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

# Verify the existance of all of the broken clients, if one doesn't exist, try to make them all.
for ((i=0,j=1; i<${#client_list[@]} && j<=2; i++)); do
    if [[ ! -x ${client_list[$i]} ]]; then

        # If this is the second time checking for the broken clients, then just error out.
        if [[ j -eq 2 ]]; then
            echo "${RED}Error: Failed making broken clients and ${client_list[$i]} still doesn't exist!${END_COLOR}"
            exit 1
        fi

        if [[ ! -e makefile ]]; then
            echo "${RED}Error: ${client_list[$i]} doesn't exist and there isn't a makefile to try and make it.${END_COLOR}"
            exit 1
        fi

        echo "Attempting to make the broken clients..."

        if ! make brokenclients; then
            echo "${RED}Error: Failed making broken clients.${END_COLOR}"
            exit 1
        fi

        # Reset the variables to check if all the files exist one more time. 
        i=0
        ((j++))
    fi
done
# End client tty make code

if lsof -i :4070 &> /dev/null; then
    echo -e "\nServer is running!"
else
    echo "Error: server does not seem to be running"
    exit 1
fi

remove_error_file

echo -e "\nTest Parameters:"
echo "--------------------------------"
echo "Number of clients = $nclients"
echo "Number of working clients = $ngclients"
echo "Number of broken clients = $ngclients"
echo "Commands to be run:" $clientcommands
echo "--------------------------------"

echo -e "\nBeginning the server flood...\n";

SECONDS=0
# Run specified number of clients against server and drop client output.:
for (( i=1; i<=$nclients; i++ )); do

    # Choose a random number between 1 and 100.
    rand=$((1 + RANDOM % 100))
    clientsleft=$((nclients-i))

    # If there are only as many clients left to test as there are broken clinets to test, then simply 
    # run the broken client since we need to make sure they all run. Otherwise, interlace broken and 
    # working clients together.
    if ([[ rand -gt 60 ]] && [[ $bclientsleft -gt 0 ]]) || ([[ $clientsleft -eq $bclientsleft ]] && [[ $bclientsleft -ne 0 ]]); then

        ((nbclientsrun++))
        csize=${#client_list[@]}    # The size of the array.
        rclient=$((rand % csize))   # Choose a random number bounded by the size of the array.
        clientscript | "$scriptdir"/${client_list[$rclient]} 127.0.0.1 &> /dev/null &
        cpid=${!}                   # Get the PID of the client that was just created.
        ((bclientsleft--))

        echo "($i/$nclients) Adding ${client_list[$rclient]} BROKEN client ($cpid) and running commands..."
    else
        ((ngclientsrun++))
        clientscript | "$scriptdir"/client-no-tty-tester 127.0.0.1 1> /dev/null 2>> testerrors &
        cpid=${!}                   # Get the PID of the client that was just created.

        echo "($i/$nclients) Adding WORKING client ($cpid) and running commands..."
    fi

    # Turn on single line writing.
    if [[ linewriting -eq 1 ]]; then
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

echo -e "Test Results:"
echo "--------------------------------"
echo "Number of clients = $nclients"
echo "Number of broken clients = $nbclientsrun"
echo "Number of working clients = $ngclientsrun"
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
    echo -e "\n${RED}Error messages generated, see file: testerrors${END_COLOR}\n"
    exit 1
else
    echo -e "\n${GREEN}Successfully passed all testing with $nclients clients!${END_COLOR}\n"
    remove_error_file
    exit 0
fi

#EOF
