#!/bin/bash

# Version 0.1
# Author: Taylor Flatt
# Script which will join each type of broken client to the server and check whether the server 
# successfully handles the case. 
#
# Note: The usage of this script will depend on how the broken clients used are coded.
#
# Usage: broken_clients_test

# Font colors for error/success messages.
RED=`tput setaf 1`
GREEN=`tput setaf 2`
END_COLOR=`tput sgr0`

ip="127.0.0.1"
errors=0
ntest=0
tests=8

if ! lsof -i :4070 &> /dev/null; then
    echo "Error: server does not seem to be running"
    exit 1
else
    echo "Server running..."
fi

echo -e "\nTest Parameters:"
echo "--------------------------------"
echo "Number of tests = $tests"
echo "--------------------------------"

# Attempt to make the clients
if ! make brokenclients; then
    echo "${RED}Failed to make the broken clients!${END_COLOR}"
    exit 1
fi

echo -e "\nBeginning client tests...\n";

((ntest++))
echo "($ntest/$tests) Processing client-wait-on-handshake..."
# Run the tests.
if ./client-wait-on-handshake $ip 1> /dev/null; then
    echo "${GREEN}Passed wait on handshake test!${END_COLOR}"
else
    echo "${RED}Failed wait on handshake test!${END_COLOR}"
    ((errors++))
fi

((ntest++))
echo "($ntest/$tests) Processing client-wait-on-connect..."
if ./client-wait-on-connect $ip 1> /dev/null; then
    echo "${GREEN}Passed wait on connect test!${END_COLOR}"
else
    echo "${RED}Failed wait on connect test!${END_COLOR}"
    ((errors++))
fi

((ntest++))
echo "($ntest/$tests) Processing client-parent-sigquit..."
if ./client-parent-sigquit $ip 1> /dev/null; then
    echo "${GREEN}Passed parent sigquit test!${END_COLOR}"
else
    echo "${RED}Failed parent sigquit test!${END_COLOR}"
    ((errors++))
fi

((ntest++))
echo "($ntest/$tests) Processing client-parent-sigint..."
if ./client-parent-sigint $ip 1> /dev/null; then
    echo "${GREEN}Passed parent sigint test!${END_COLOR}"
else
    echo "${RED}Failed parent sigint test!${END_COLOR}"
    ((errors++))
fi

((ntest++))
echo "($ntest/$tests) Processing client-failed-secret..."
if ./client-failed-secret $ip 1> /dev/null; then
    echo "${GREEN}Passed client failed secret!${END_COLOR}"
else
    echo "${RED}Failed client failed secret!${END_COLOR}"
    ((errors++))
fi

((ntest++))
echo "($ntest/$tests) Processing client-failed-proceed..."
if ./client-failed-proceed $ip 1> /dev/null; then
    echo "${GREEN}Passed client failed proceed!${END_COLOR}"
else
    echo "${RED}Failed client failed proceed!${END_COLOR}"
    ((errors++))
fi

((ntest++))
echo "($ntest/$tests) Processing client-failed-port..."
if ./client-failed-port $ip 1> /dev/null; then
    echo "${GREEN}Passed client failed port!${END_COLOR}"
else
    echo "${RED}Failed client failed port!${END_COLOR}"
    ((errors++))
fi

((ntest++))
echo "($ntest/$tests) Processing client-failed-challenge..."
if ./client-failed-challenge $ip 1> /dev/null; then
    echo "${GREEN}Passed client failed challenge!${END_COLOR}"
else
    echo "${RED}Failed client failed challenge!${END_COLOR}"
    ((errors++))
fi

if [ "$errors" -gt "0" ]; then
    echo -e "\n${RED}Failed one or more tests.${END_COLOR}\n"
    exit 1
else
    echo -e "\n${GREEN}Successfully passed all tests!${END_COLOR}\n"
    exit 0
fi


