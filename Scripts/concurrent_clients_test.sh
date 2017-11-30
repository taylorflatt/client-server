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
# Usage: floodServer -n NUM100CLIENTS -c NUMCYCLES

function print_usage()
{
	echo -e "\nUsage: $0 -n NUM_GOOD_CLIENTS -c NUM_BAD_CLIENTS [-hl]\n"
}

# Displays the help information to a user.
function print_help()
{
	print_usage
	echo -e "Joins n-batches of 100 clients to the server and run a string of commands m-times (m cycles). \n"
	
	echo -e "\t -n NUM_100_CLIENTS \t The number of batches of 100 clients who will run c-cycles."
	echo -e "\t -c NUM_CYCLES \t\t The number of times the command sequence is run on each client."
    echo -e "\t -l \t\t\t Enables line rewriting so the terminal isn't saturated with the client creation information."
    echo -e "\t -h \t\t\t Prints this help message. \n"
	
	echo "Note:"
	echo -e "-It may take some time for the script to finish executing since it waits for all of the clients to exit prior to returning. However, if it doesn't exit, then there is a problem with the server.\n"
	
	echo "Example:"
	echo -e "\t./concurrent_clients_test -n 100 -c 5 \n"
	echo -e "\tWill create 100 batches of 100 clients totalling 10000 clients created running the sequence of commands 5 times each.\n"
	
	echo -e "Full documentation and source code can be found at: <www.github.com/taylorflatt/client-server>.\n"
}

# Font colors for error/success messages.
RED=`tput setaf 1`
GREEN=`tput setaf 2`
END_COLOR=`tput sgr0`

# Sequence of commands to be sent to server:
# (Separate each command line with \n, exit automatically sent at end.)
clientcommands=$'pwd\ncd\npwd\nls -l'

if [[ $# < 1 || $# > 6 ]]; then
    print_usage
    exit 1
fi

#nclients=
#ncycles=
linewriting=0
bsize=100
client=0

# Parse the arguments.
while getopts ":n::c:lh" opt; do
	case $opt in
    n)
		nclients=$OPTARG
		;;
    c)
		ncycles=$OPTARG
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

# Make sure that the number of good and bad clients are set. This uses variable expansion 
# which will overwrite the variable ngclients if it is set, otherwise it evaluates to nothingness
# if ngclients is not set.
if [[ -z ${nclients+x} ]]; then
    echo -e "\n${RED}Error: Please specify the number of client batches!${END_COLOR}"
    print_usage
    exit 1
fi

if [[ -z ${ncycles+x} ]]; then
    echo -e "\n${RED}Error: Please specify the number of cycles!${END_COLOR}"
    print_usage
    exit 1
fi

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
if [[ ! -e client-no-tty-tester ]]; then
    if ! make nottyclient; then 
        echo "${RED}Error: Failed making client-no-tty-tester.${END_COLOR}"
        exit 1
    fi
fi
# End client tty make code

if [[ ! -x client-no-tty-tester ]]; then
    echo "${RED}The client-no-tty-tester either doesn't exist or isn't executable!${END_COLOR}"
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
echo "Commands to be run:" $clientcommands
echo "--------------------------------"

echo -e "\nBeginning client tests...\n";

# Run specified number of clients against server:
for (( i=1; i<="$nclients"; i++ )); do
    echo "Processing batch $i"
    for (( j=1; j<=$bsize; j++)); do 
        ((client++))   # Update to the current client.

        clientscript "$ncycles" $client | ./client-no-tty-tester 127.0.0.1 2>> testerrors 1> /dev/null &
        cpid=${!}
        
        echo "($client/$(($nclients * $bsize))) Adding client ($cpid) and running commands..."

        if [[ linewriting -eq 1 ]]; then
            tput civis      # Set the cursor to invisible momentarily.
            tput cuu1       # Move the cursor up one line.
            tput el         # Delete the entire line in the terminal.
            tput cnorm      # Set the cursor back to being visible.
        fi
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
