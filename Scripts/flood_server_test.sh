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

# Font colors for error/success messages.
RED=`tput setaf 1`
GREEN=`tput setaf 2`
END_COLOR=`tput sgr0`

# Sequence of commands to be sent to server:
# (Separate each command line with \n, exit automatically sent at end.)
clientcommands=$'pwd\nexit'

if [[ $# != 1 ]]; then
    echo "flood_server_test NUMCLIENTS"
    exit 1
fi

scriptdir=$(dirname "$0")
nclients=$1

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
for (( i=1; i<="$nclients"; i++ )); do
    
    clientscript | "$scriptdir"/client-no-tty-tester 127.0.0.1 &> /dev/null &

    echo "($i/$nclients) Adding client and running commands..."
    tput civis      # Set the cursor to invisible momentarily.
    tput cuu1       # Move the cursor up one line.
    tput el         # Delete the entire line in the terminal.
    tput cnorm
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
