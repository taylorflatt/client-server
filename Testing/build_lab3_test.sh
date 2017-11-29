#!/bin/bash

# Move into the testing directory so everything is local.
cd Testing

if ! make lab3client; then 
    echo "Error making lab 3 client."
    exit 1
fi

if ! make lab3server; then
    echo "Error making lab 3 server."
    exit 1
fi

exit 0