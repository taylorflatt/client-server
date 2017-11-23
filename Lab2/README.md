
# Lab 2
Completed on September 25th.

## Description
A primitive concurrent client-server which creates many subprocesses to handle client communication. The assumption is that all writes are full; thus no partial writes are handled. One primary distinction between this implementation and the previous implementation is that bash is setup with its own PTY and a complex structure of processes handle communication between the client and server. Additionally, there are now debug statements included into the implementation which can be enabled with the ```-DDEBUG``` flag set.

## Features

| Feature                   | Implements    |
| ------------------------- |:-------------:|
| Processes Created         | 3             |
| Threads Created           | 0             |
| Concurrent Server         | YES           |
| Three-way Handshake       | YES           |
| IO Type                   | Blocking      |
| Debug Mode                | YES           |
| Signal Handler            | YES           |
| Handles Malicious Clients | NO            |
| Handles Partial Writes    | NO            |
| Thread Pool               | NO            |
| Epoll                     | NO            |

## Usage
In order to compile the client run:

```
make client
```

In order to compile the client with debugging turned on, run:

```
make clientD
```

In order to compile the server:

```
make server
```

In order to compile the server with debugging turned on, run:

```
make serverD
```

Note: If make is not installed, you can install it using ```sudo apt-get install make``` or simply look inside the makefile and copy the gcc command.