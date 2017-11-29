#!/bin/bash

# Move into the testing directory so everything is local.
cd Testing

if ! make brokenclients; then 
    echo "Failed to make at least 1 broken client."
    exit 1
fi

exit 0