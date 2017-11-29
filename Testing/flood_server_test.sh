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
clientcommands=$'pwd\n'

if [[ $# != 1 ]]; then
    echo "flood_server_test NUMCLIENTS"
    exit 1
fi

# Move into the testing directory.
cd Testing

scriptdir=$(dirname "$0")
nclients=$1
bsize=100

function clientscript()
{
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
            echo "Failed to remove any previous testerrors files! Manually remove the file to continue."
            return -1
        fi
    fi

    return 0
}

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

if ! make nottyserver; then
    echo "Error: Unable to make the newest server version"
    exit 1
fi

if lsof -i :4070 &> /dev/null; then
    echo "Error: Server is already running! It must be killed before this can be started."
    exit 1
fi

# Run the server and capture its PID so it can be killed later.
./server 2>> testerrors &
serverpid=$!

if ! lsof -i :4070 &> /dev/null; then
    echo "Error: server does not seem to be running"
    exit 1
fi

remove_error_file

# Run specified number of clients against server and drop client output.:
for (( i=1; i<="$nclients"; i++ )); do
    echo "($i/$nclients) Adding client and running commands..."
    clientscript | "$scriptdir"/client-no-tty-tester 127.0.0.1 &> /dev/null &
    #sleep 1
done

echo "Done Testing!"

# Some errors may be output during the kill/killall but these are fine. So long as no errors 
# were encountered during the runtime of flooding the server with clients, things are working 
# fine.
if [[ -s testerrors ]]; then
    echo "${RED}Error messages generated, see file: testerrors${END_COLOR}"
    killall client-no-tty-tester
    kill $serverpid
    exit 1
else
    echo "${GREEN}Successfully passed all testing with $(($nclients * $bsize)) clients each executing $ncycles cycles!${END_COLOR}"
    remove_error_file
    killall client-no-tty-tester
    kill $serverpid
    exit 0
fi

#EOF
