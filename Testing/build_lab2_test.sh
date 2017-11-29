#!/bin/bash

# Move into the testing directory so everything is local.
cd Testing

if ! make lab2client; then 
    echo "Error making lab 2 client."
    exit 1
fi

if ! make lab2server; then
    echo "Error making lab 2 server."
    exit 1
fi

exit 0