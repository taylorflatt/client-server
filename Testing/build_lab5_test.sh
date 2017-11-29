#!/bin/bash

# Move into the testing directory so everything is local.
cd Testing

if ! make lab5client; then 
    echo "Error making lab 5 client."
    exit 1
fi

if ! make lab5server; then
    echo "Error making lab 5 server."
    exit 1
fi

exit 0