
# Lab 3
Completed on October 30th.

## Description
A concurrent client-server which creates many subprocesses to handle client communication. The assumption is that all writes are full; thus no partial writes are handled. One primary distinction between this implementation and the previous implementation is that clients are handled in the server using a single pthread and all communication is done using a single pthread (for all clients).

## Features

| Feature                   | Implements    |
| ------------------------- |:-------------:|
| Processes Created         | -             |
| Threads Created           | -             |
| Concurrent Server         | -             |
| Three-way Handshake       | -             |
| IO Type                   | -             |
| Debug Mode                | YES           |
| Signal Handler            | NO            |
| Handles Malicious Clients | -             |
| Handles Partial Writes    | -             |
| Thread Pool               | YES           |
| Epoll                     | -             |

## Usage
In order to compile the thread pool with all libraries run:

```
make pool
```

In order to compile the thread pool with all libraries and debugging, run:

```
make poolD
```

Running the make commands will also compile a test program which will stress test the thread pool.

Note: If make is not installed, you can install it using ```sudo apt-get install make``` or simply look inside the makefile and copy the gcc command.