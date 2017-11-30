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
# Usage: floodServer NUM100CLIENTS NUMCYCLES

# Font colors for error/success messages.
RED=`tput setaf 1`
GREEN=`tput setaf 2`
END_COLOR=`tput sgr0`

# Sequence of commands to be sent to server:
# (Separate each command line with \n, exit automatically sent at end.)
clientcommands=$'pwd\ncd\npwd\nls -l'

if [[ $# != 2 ]]; then
    echo "flood_server_test NUM100CLIENTS NUMCYCLES"
    exit 1
fi

nclients=$1
ncycles=$2
bsize=100
client=0

function clientscript() {
    # Change the separator from spaces to a newline 
    # so we can send commands that include spaces.
    IFS=$'\n'
    echo "unset HISTFILE"
    for d in $(seq 1 "$nclients"); do
        for cmd in $clientcommands; do
            echo $cmd
            sleep 1
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
            echo "${RED}Failed to remove any previous testerrors files! Manually remove the file to continue."
            return -1
        fi
    fi

    return 0
}

# This make code can be removed if you don't want it to make the client tty.
if [[ -e client-no-tty-tester ]]; then
    if ! rm -f client-no-tty-tester; then
        echo "${RED}Failed to remove old version of the client.${END_COLOR}"
        exit 1
    fi
fi

if ! make nottyclient; then 
    echo "${RED}Error: Failed making client-no-tty-tester.${END_COLOR}"
    exit 1
fi
# End client tty make code

if [[ ! -x client-no-tty-tester ]]; then
    echo "${RED}The client executable client-no-tty-tester doesn't exist!${END_COLOR}"
    exit 1
fi

if lsof -i :4070 &> /dev/null; then
    echo "Server is running!"
else
    echo "${RED}Error: server does not seem to be running!${END_COLOR}"
    exit 1
fi

remove_error_file

echo -e "\nTest Parameters:"
echo "--------------------------------"
echo "Number of batches = $nclients"
echo "Number of clients = $(($nclients * $bsize))"
echo "--------------------------------"

echo -e "\nBeginning client tests...\n";

# Run specified number of clients against server:
for (( i=1; i<="$nclients"; i++ )); do
    echo "Processing batch $i"
    for (( j=1; j<=$bsize; j++)); do 
        (($client++))   # Update to the current client.

        clientscript "$ncycles" $client | ./client-no-tty-tester 127.0.0.1 2>> testerrors 1> /dev/null &
        
        echo "($client/$(($nclients * $bsize))) Adding client and running commands..."
        tput civis      # Set the cursor to invisible momentarily.
        tput cuu1       # Move the cursor up one line.
        tput el         # Delete the entire line in the terminal.
        tput cnorm      # Set the cursor back to being visible.
    done
    echo "Finished processing batch $i"
    sleep 1
done

echo -e "\nWaiting for clients to exit..."

# Waits for all children to close prior to exiting. If the 
# script hangs, then all children aren't finishing/exiting.
# That would be an error.
wait

echo -e "\nDone Testing!\n"

if [[ -s testerrors ]]; then
    echo -e "${RED}Error messages generated, see file: testerrors${END_COLOR}\n"
    exit 1
else
    echo -e "${GREEN}Successfully passed all testing with $(($nclients * $bsize)) clients each executing $ncycles cycles!${END_COLOR}\n"
    remove_error_file
    exit 0
fi

#EOF
