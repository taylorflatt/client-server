#!/bin/bash

# Move into the testing directory so everything is local.
cd Testing

if ! make lab4tpool; then 
    echo "Error making lab 4 thread pool files."
    exit 1
fi

exit 0