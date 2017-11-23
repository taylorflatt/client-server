
# Lab 3
Completed on October 10th.

## Description
A concurrent client-server which creates many subprocesses to handle client communication. The assumption is that all writes are full; thus no partial writes are handled. One primary distinction between this implementation and the previous implementation is that clients are handled in the server using a single pthread and all communication is done using a single pthread (for all clients).

## Features

| Feature                   | Implements    |
| ------------------------- |:-------------:|
| Processes Created         | 1             |
| Threads Created           | 2             |
| Concurrent Server         | YES           |
| Three-way Handshake       | YES           |
| IO Type                   | Blocking      |
| Debug Mode                | YES           |
| Signal Handler            | YES           |
| Handles Malicious Clients | YES*          |
| Handles Partial Writes    | NO            |
| Thread Pool               | NO            |
| Epoll                     | YES           |

*<sup><sub>Handles malicious clients by using timers whereas Lab5 uses timerfd with its own epoll unit.</sub></sup>

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

In order to run or test with malfunctioning/malicious clients, run the makefile in the [Broken Clients](https://github.com/taylorflatt/client-server/tree/master/Lab3/BrokenClients) directory.

```
make allclients
```

Similarly for the debugging option:

```
make allclientsD
```

Note: If make is not installed, you can install it using ```sudo apt-get install make``` or simply look inside the makefile and copy the gcc command.