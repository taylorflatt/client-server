
# Testing Scripts

## Description
These scripts can be used to test different functionality for the clients and servers. If running tests manually, these are the scripts that ought to be used.

The makefile included should accompany the concurrent_clients_test, otherwise, substitute gcc commands in for the make command.

## Usage

### Broken Clients Test
The script tests all of the broken clients in the BrokenClients directory by running them against an existing server. The script will attempt to make the files with ```make brokenclients```. Be sure to have a makefile in the same directory as the script with the correct syntax (depending on where you placed the broken clients).

Start the server:
```
./server
```

Start the script:
```
./broken_clients_test.sh
```

The script has a documented portion of code which can be removed that will skip the make for broken clients if that is ever an issue.

### Concurrent Clients Test
The script will test batches of clients against the server by connecting them quickly and running some basic commands to verify multiple clients can connect and simultaneous IO can occur. Finally, the script will wait until all of the clients are collected before exiting.

The script takes two arguments:
- _NUM_BATCHES_: The number of batches of 100 clients that will be created to run on the server.
- _NUM_CYCLES_: The number of types the sequence of commands will be run by each client.

Usage:
```
./concurrent_clients_test.sh NUM_BATCHES NUM_CYCLES
```

In order to properly run the script, perform the following actions:

Start the server:
```
./server
```

Start the script:
```
./concurrent_clients_test 10 5
```

The above command will run 10 rounds of 100 clients running a set of commands 5 times each on the server before exiting.

### Flood Server Test
The script rapidly connects clients to the server in an attempt to stress test the connection process. The clients will run a single command to make sure they are fully connecting and can communicate with bash. Finally, the script will wait until all of the clients are collected before exiting. If the script hangs and your server isn't doing anything, there is definitely a problem.

The script takes a single argument:
- NUM_CLIENTS: The number of clients to connect to the server quickly.

Usage:
```
./flood_server_test.sh NUM_CLIENTS
```

In order to properly run the script, perform the following actions:

Start the server:
```
./server
```

Start the script:
```
./flood_server_test.sh 1000
```

The above command will create 1000 clients on the server.

### Flood Server Hybrid Test
The script rapidly connects BOTH broken and working clients to the server in an attempt to stress test the connection process. The working clients will run a single command to make sure they are fully connecting and can communicate with bash. Finally, the script will wait until all of the clients are collected before exiting. If the script hangs and your server isn't doing anything, there is definitely a problem.

The script takes a three arguments:
- -g NUM_GOOD_CLIENTS: The number of working clients to connect to the server quickly.
- -b NUM_BAD_CLIENTS: The number of broken clients to connect to the server quickly.
- -l: Runs the script in line squashing mode where it will show the current progress on a single line rather than show the runtime tests on all newlines. Useful for a quick check to see if it runs or not. Otherwise, you will likely want to keep this option off so you can see the PIDs of the processes and debug easier.

Usage:
```
./flood_server_hybrid_test.sh -g NUM_GOOD_CLIENTS -b NUM_BAD_CLIENTS [-l]
```

In order to properly run the script, perform the following actions:

Start the server:
```
./server
```

Start the script:
```
./flood_server_hybrid_test.sh -g 150 -b 50
```

The above command will create 200 clients on the server where 150 of them are working clients and 50 are broken clients. Note that they will all be interlaced together and not grouped.

### PWrite
The script is just a small helper to cat a file over and over to create a partial write.

**Note**: This is in the process of being worked into a larger script which will create a partial write and verify the output.

## Issues
I have verified functionality of the scripts on my setup. However, that doesn't necessarily mean it will work for you. I suggest giving the scripts a look over before using them.