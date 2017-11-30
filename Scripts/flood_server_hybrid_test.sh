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
	echo "Usage: $0 -g NUM_GOOD_CLIENTS -b NUM_BAD_CLIENTS [-l]"
}

# Font colors for error/success messages.
RED=`tput setaf 1`
GREEN=`tput setaf 2`
END_COLOR=`tput sgr0`

# Check arguments.
if [[ $# < 2 || $# > 5 ]]; then
    print_usage
    exit 1
fi

clientcommands=$'pwd\nexit'         # Sequence of commands to be sent to server.
scriptdir=$(dirname "$0")           # Get the directory the script is located in.
linewriting=0                       # Flag as to whether single line writing is on.
nbclientsrun=0                      # Number of bad clients that have been created.
ngclientsrun=0                      # Number of good clients that have been created.
ngclients=                          # Number of good clients.
nbclients=                          # Number of bad clients.

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
while getopts ":g::b:l" opt; do
	case $opt in
    g)
		ngclients=$OPTARG
        echo "ngclients was set to: $OPTARG"
		;;
    b)
		nbclients=$OPTARG
        echo "nbclients was set to: $OPTARG"
		;;
	l)
        echo "Setting line writing option!"
		linewriting=1
		;;
	?)
		print_usage
		exit 1	
		;;
	esac
done

nclients=$((ngclients + nbclients)) # Number of total clients.
bclientsleft=$nbclients             # Number of bad clients yet to be created.

if [[ -z ${ngclients+x} ]]; then
    echo "Error: Please specify the number of good clients!"
    exit 1
fi

if [[ -z ${nbclients+x} ]]; then
    echo "Error: Please specify the number of bad clients!"
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

# This make code can be removed if you don't want it to make the client tty.
if [[ ! -x client-no-tty-tester ]]; then
    if ! make nottyclient; then 
        echo "Error: Failed making client-no-tty-tester."
        exit 1
    fi

    if [[ ! -x client-no-tty-tester ]]; then
        echo "Error: Must have a client named client-no-tty-tester."
        exit 1
    fi
fi

if ! make brokenclients; then
    echo "Error: Failed making broken clients."
    exit 1
fi
# End client tty make code

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
        clientscript | "$scriptdir"/client-no-tty-tester 127.0.0.1 &> /dev/null &
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
