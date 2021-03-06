#!/bin/bash

# Version 0.1
# Author: Taylor Flatt
# Script which will run a cat command on a set of files c-times and verify the output. A client is 
# joined to the server and then exits for each command to ensure that there isn't random buffer before utilized 
# for subsequent test files. 
#
# IMPORTANT: This REQUIRES you change the number of bytes read by the server to 2000 from the 
#            nread (what was read by the read command). 
#
#               Example: 
#                   Original -> nwrite = write(to, buf, nread); // Where nread = num of bytes read.
#                   New      -> nwrite = write(to, buf, 2000);
#
#
# Note: The script also calls the client-no-tty client since running headless, we don't want to 
# make modifications to the tty.
#
# Usage: partial_write_cycle_test.sh -c NUMCYCLES

function print_usage()
{
	echo -e "\nUsage: $0 -c NUMCYCLES [-l]\n"
}

# Displays the help information to a user.
function print_help()
{
	print_usage
	echo -e "Joins a single client which will run a cat command on a set of files a c-times and verify the output. \n"
	
    echo -e "\t -c NUMCYCLES \t The number of times a single client will cat the test file for their particular test."
    echo -e "\t -l \t\t Enables line rewriting so the terminal isn't saturated with the client creation information."
    echo -e "\t -h \t\t Prints this help message. \n"
	
	echo "Note:"
	echo -e "\t-It may take some time for the script to finish executing since it waits for all of the clients to exit prior to returning. However, if it doesn't exit, then there is a problem with the server.\n"
	
	echo "Example:"
	echo -e "\t./partial_write_cycle_test -c 2 \n"
	echo -e "\tWill create a new client for each test that will run the test 2 times.\n"
	
	echo -e "Full documentation and source code can be found at: <www.github.com/taylorflatt/client-server>.\n"
}

# Font colors for error/success messages.
RED=`tput setaf 1`
GREEN=`tput setaf 2`
YELLOW=`tput setaf 3`
END_COLOR=`tput sgr0`

if [[ $# < 1 || $# > 3 ]]; then
    print_usage
    exit 1
fi

scriptdir=$(dirname "$0")
linewriting=0
ntests=8
failed=0
cmd=
outputfile=
cycles=

# Parse the arguments.
while getopts ":c:lh" opt; do
	case $opt in
    c)
        cycles=$OPTARG
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

# Make sure that the number of cat cycles are set. This uses variable expansion 
# which will overwrite the variable ngclients if it is set, otherwise it evaluates to nothingness
# if ngclients is not set.
if [[ -z ${cycles+x} ]]; then
    echo -e "\n${RED}Error: Please specify the number of cycles!${END_COLOR}"
    print_usage
    exit 1
fi

# The test files MUST contain the following structure:
# byteSize.test (Must be exactly .test at the end).
declare -a test_list
test_list+=("500.test")
test_list+=("1000.test")
test_list+=("2000.test")
test_list+=("3000.test")
test_list+=("4000.test")
test_list+=("10000.test")
test_list+=("20000.test")
test_list+=("100000.test")

# Make sure the user would like to generate the number of files required for the test
# and is aware of the size of the generated files as well.
if [[ $cycles -gt 10 ]]; then
total=
for (( i=0; i<${#test_list[@]}; i++ )); do
    filesize=${test_list[$i]::-5}
    total=$((total + filesize))
done

while true; do
    echo ""
    prompt="${YELLOW}WARNING! The number of cycles you have entered will create $((ntests * cycles)) "
    prompt+="files for output with a total size around $(((total * cycles) / 100000))MB! ARE YOU SURE (y/n)?${END_COLOR} "
    read -p "$prompt" ans

    case ${ans:0:1} in
        y|Y )
            echo -e "\nContinuing...\n"
            break
            ;;
        n|N )
            echo -e "\nNo changes made. Exiting...\n"
            exit 0
            ;;
        * )
            echo -e "\nPlease enter either a Y/y or a N/n."
            continue
        ;;
    esac
done
fi

# Setup to allow this function's output to be redirected to /dev/null 
# and dynamically change IO redirection so that the output to the file 
# is ONLY what is written as output to the cat command.
function clientscript()
{
    # Change the separator from spaces to a newline 
    # so we can send commands that include spaces.
    IFS=$'\n'

    # If the server is in another directory, you need to move the client
    # into the directory of the script so it can run the diff cmd later.
    echo "cd ../Scripts"

    echo "unset HISTFILE"
    for c in $(seq 1 "$cycles"); do
            echo "$cmd 1> $outputfile.$c"
    done

    exit_client 1> /dev/null

    #Keep stdin open briefly:
    sleep 1

    exit
}

function exit_client() {

    echo "exit"
}

# Check if the client exists. If it doesn't, build it and check again.
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

# Check to see if the test files exist and if the output files exist (remove them).
for ((i=0; i<${#test_list[@]}; i++)); do

    # Strip the .file from the filename and the append .output.
    outputfile="${test_list[$i]::-5}.output"

    if [[ ! -e ${test_list[$i]} ]]; then
        echo "${RED}Error: Failed to locate ${test_list[$i]} in the current directory. Please move the file to $(pwd)${END_COLOR}"
        exit 1
    fi

    ofiles=$(ls $outputfile* 2> /dev/null | wc -l)
    if [[ $ofiles -ne 0 ]]; then
        files=($outputfile*)
        echo -e "\nRemoving the old output file(s) for ${outputfile}..."

        for (( j=0; j<${#files[@]}; j++ )); do
            echo "Removing ${files[$j]}..."
            if ! rm -f ${files[$j]}; then
                echo "${RED}Failed to remove $ofiles[$j]. Please remove it before continuing.${END_COLOR}"
                exit 1
            fi

            if [[ -e ${files[$j]} ]]; then
                echo "${RED}Failed to remove ${files[$j]}!${END_COLOR}"
                exit 1
            else
                echo "Successfully removed ${files[$j]}"
            fi
        done
    fi
done

echo -e "\nFinished cleaning up old output files..."

if lsof -i :4070 &> /dev/null; then
    echo -e "Server is running!"
else
    echo "${RED}Error: server does not seem to be running.${END_COLOR}"
    exit 1
fi

echo -e "\nTest Parameters:"
echo "--------------------------------"
echo "Number of tests = $((ntests * cycles))"
echo "Number of cycles = $cycles"
echo "--------------------------------"

echo -e "\nBeginning the partial write tests...\n";

SECONDS=0
for (( i=0; i<${#test_list[@]}; i++ )); do

    echo -e "Starting test: ${test_list[$i]}"

    cmd="cat ${test_list[$i]}"
    outputfile="${test_list[$i]::-5}.output"

    # Redirect everything to nothing so that the client connection and bash information isn't 
    # added to the output file for diffing.
    clientscript | "$scriptdir"/client-no-tty-tester 127.0.0.1 &> /dev/null

    # Check if the output matches the input.
    for c in $(seq 1 "$cycles"); do
        if ! diff -q ${test_list[$i]} $outputfile.$c &> /dev/null; then
            echo -e "${RED}Test ${test_list[$i]} FAILED! The output differs from the input! The changes are below:${END_COLOR}\n"
            ((failed++))
            diff ${test_list[$i]} $outputfile.$c
        else
            echo -e "${GREEN}Test ${test_list[$i]} PASSED!${END_COLOR}\n"
        fi
    done


    # Make sure there is ample time between clients.
    sleep 1
done

echo -e "\nWaiting for clients to exit..."

# Waits for all children to close prior to exiting. If the 
# script hangs, then all children aren't finishing/exiting.
# That would be an error.
wait

echo -e "\nDone Testing!\n"

echo -e "\nTest Results:"
echo "--------------------------------"
echo "Successful tests = $(((ntests * cycles) - failed))"
echo "Failed tests = $failed"
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
if [[ $failed -ne 0 ]]; then
    echo -e "${RED}Failed to pass all tests.${END_COLOR}\n"
    exit 1
else
    echo -e "${GREEN}Successfully passed all testing with clients!${END_COLOR}\n"
    exit 0
fi

#EOF

