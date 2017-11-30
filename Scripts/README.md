
# Testing Scripts

## Description
These scripts can be used to test different functionality for the clients and servers. If running tests manually, these are the scripts that ought to be used.

The makefile included should accompany the concurrent_clients_test, otherwise, substitute gcc commands in for the make command.

## Usage
Short note about the broken clients. When they are required, they are referenced as being in the _same_ directory as the script that is running. Hence, it is important to have them in the same directory or you will have issues.

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

The script can take multiple arguments:
- -n _NUM_BATCHES_: The number of batches of 100 clients that will be created to run on the server.
- -c _NUM_CYCLES_: The number of types the sequence of commands will be run by each client.
- -l: Enables line rewriting so the terminal isn't saturated with the client creation information.
- -h: Prints the help message.

Usage:
```
./concurrent_clients_test.sh -n NUM_BATCHES -c NUM_CYCLES [-hl]
```

In order to properly run the script, perform the following actions:

Start the server:
```
./server
```

Start the script:
```
./concurrent_clients_test -n 10 -c 5
```

The above command will run 10 rounds of 100 clients running a set of commands 5 times each on the server before exiting.

### Flood Server Test
The script rapidly connects clients to the server in an attempt to stress test the connection process. The clients will run a single command to make sure they are fully connecting and can communicate with bash. Finally, the script will wait until all of the clients are collected before exiting. If the script hangs and your server isn't doing anything, there is definitely a problem.

The script can take multiple arguments:
- -n NUM_CLIENTS: The number of clients to connect to the server quickly.
- -l: Enables line rewriting so the terminal isn't saturated with the client creation information.
- -h: Prints the help message.

Usage:
```
./flood_server_test.sh -n NUM_CLIENTS [-lh]
```

In order to properly run the script, perform the following actions:

Start the server:
```
./server
```

Start the script:
```
./flood_server_test.sh -n 1000
```

The above command will create 1000 clients on the server.

### Flood Server Hybrid Test
The script rapidly connects BOTH broken and working clients to the server in an attempt to stress test the connection process. The working clients will run a single command to make sure they are fully connecting and can communicate with bash. Finally, the script will wait until all of the clients are collected before exiting. If the script hangs and your server isn't doing anything, there is definitely a problem.

The script takes a three arguments:
- -g NUM_GOOD_CLIENTS: The number of working clients to connect to the server quickly.
- -b NUM_BAD_CLIENTS: The number of broken clients to connect to the server quickly.
- -l: Runs the script in line squashing mode where it will show the current progress on a single line rather than show the runtime tests on all newlines. Useful for a quick check to see if it runs or not. Otherwise, you will likely want to keep this option off so you can see the PIDs of the processes and debug easier.
- -h: Prints the help message.

Usage:
```
./flood_server_hybrid_test.sh -g NUM_GOOD_CLIENTS -b NUM_BAD_CLIENTS [-lh]
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

### Show Server
Shows all of the clients called "server" which are currently running using the ps command. It also shows all of the open threads and processes.

To run the script, simply cd into its directory and run:
```
./show-server
```

### Show Clients
Shows all of the clients called "client" which are currently running for a server using the ps command. It also shows all of the open threads and processes.

To run the script, simply cd into its directory and run:
```
./show-client
```

### Show Sockets
Shows all of the sockets open on the port the server is using. Default is set to 4070. If the default port is changed within the server, it must be changed within the script.

To run the script, simply cd into its directory and run:
```
./show-sockets
```

### PWrite
The script is just a small helper to cat a file over and over to create a partial write.

**Note**: This is in the process of being worked into a larger script which will create a partial write and verify the output.

## Issues
I have verified functionality of the scripts on my setup. However, that doesn't necessarily mean it will work for you. I suggest giving the scripts a look over before using them.