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
    echo "testserver NUM100CLIENTS NUMCYCLES"
    exit 1
fi

scriptdir=$(dirname "$0")
nclients=$1
ncycles=$2
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

    if [[ -x testerrors ]]; then
        if ! rm -f testerrors; then
            echo "Failed to remove any previous testerrors files! Manually remove the file to continue."
            return -1
        fi
    fi

    return 0
}

if [[ ! -x client-no-tty-tester ]]; then
    echo "Error: must have client executable named: client-notty"
    exit 1
fi

if ! lsof -i :4070 &> /dev/null; then
    echo "Error: server does not seem to be running"
    exit 1
fi

if [[ remove_error_file -ne 0 ]]; then
    exit 1
fi

# Run specified number of clients against server:
for (( i=1; i<="$nclients"; i++ )); do
    for (( j=1; j<=$bsize; j++)); do 
        clientscript "$ncycles" | "$scriptdir"/client-no-tty-tester 127.0.0.1 2>> testerrors &
    done
    sleep 1
done

# Waits for all children to close prior to exiting. If the 
# script hangs, then all children aren't finishing/exiting.
# That would be an error.
wait

echo "Done Testing!"

if [[ -s testerrors ]]; then
    echo "${RED}Error messages generated, see file: testerrors${END_COLOR}"
    exit 1
else
    echo "${GREEN}Successfully passed all testing with $(($nclients * $bsize)) clients each executing $ncycles cycles!${END_COLOR}"
    remove_error_file
    exit 0
fi

#EOF
