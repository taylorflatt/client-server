#!/bin/bash

# Make and start the server.
cd Lab5 && make serverD
./server &

ip="127.0.0.1"
errors=0

# Make the clients.
cd BrokenClients && make allclientsD 

# Run the tests.
if ./client-wait-on-handshake $ip; then
    echo "Passed wait on handshake test!"
else
    echo "Failed wait on handshake test!"
    ((errors++))
fi

if ./client-wait-on-connect $ip; then
    echo "Passed wait on connect test!"
else
    echo "Failed wait on connect test!"
    ((errors++))
fi

if ./client-parent-sigquit $ip; then
    echo "Passed parent sigquit test!"
else
    echo "Failed parent sigquit test!!"
    ((errors++))
fi

if ./client-parent-sigint $ip; then
    echo "Passed parent sigint test!"
else
    echo "Failed parent sigint test!!"
    ((errors++))
fi

if ./client-failed-secret $ip; then
    echo "Passed client failed secret!"
else
    echo "Failed client failed secret!"
    ((errors++))
fi

if ./client-failed-proceed $ip; then
    echo "Passed client failed proceed!"
else
    echo "Failed client failed proceed!"
    ((errors++))
fi

if ./client-failed-port $ip; then
    echo "Passed client failed port!"
else
    echo "Failed client failed port!"
    ((errors++))
fi

if ./client-failed-challenge $ip; then
    echo "Passed client failed challenge!"
else
    echo "Failed client failed challenge!"
    ((errors++))
fi

if [ "$errors" -gt "0" ]; then
    echo "Failed one or more tests."
    trap "exit" INT TERM
    trap "kill 0" EXIT
    exit 1
else
    echo "Successfully passed all tests!"
    trap "exit" INT TERM
    trap "kill 0" EXIT
    exit 0
fi


