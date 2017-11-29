#!/bin/bash

# Move into the testing directory so everything is local.
cd Testing

if ! make lab1client; then 
    echo "Error making lab 1 client."
    exit 1
fi

if ! make lab1server; then
    echo "Error making lab 1 server."
    exit 1
fi

exit 0