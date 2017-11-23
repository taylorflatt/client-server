#!/bin/bash

# Start the server.
./server &

ip="127.0.0.1"
errors=0

# Run the tests.
if BrokenClients/client-wait-on-handshake $ip; then
    echo "Passed wait on handshake test!"
else
    echo "Failed wait on handshake test!"
    ((errors++))
fi

if BrokenClients/client-wait-on-connect $ip; then
    echo "Passed wait on connect test!"
else
    echo "Failed wait on connect test!"
    ((errors++))
fi

if BrokenClients/client-parent-sigquit $ip; then
    echo "Passed parent sigquit test!"
else
    echo "Failed parent sigquit test!!"
    ((errors++))
fi

if BrokenClients/client-parent-sigint $ip; then
    echo "Passed parent sigint test!"
else
    echo "Failed parent sigint test!!"
    ((errors++))
fi

if BrokenClients/client-failed-secret $ip; then
    echo "Passed client failed secret!"
else
    echo "Failed client failed secret!"
    ((errors++))
fi

if BrokenClients/client-failed-proceed $ip; then
    echo "Passed client failed proceed!"
else
    echo "Failed client failed proceed!"
    ((errors++))
fi

if BrokenClients/client-failed-port $ip; then
    echo "Passed client failed port!"
else
    echo "Failed client failed port!"
    ((errors++))
fi

if BrokenClients/client-failed-challenge $ip; then
    echo "Passed client failed challenge!"
else
    echo "Failed client failed challenge!"
    ((errors++))
fi

if [ "$errors" -eq "0" ]; then
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


