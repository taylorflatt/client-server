
# Lab 1
Completed on September 11th.

## Description
A primitive concurrent client-server which creates subprocesses to handle client communication. The assumption is that all writes are full; thus no partial writes are handled.

## Features

| Feature                   | Implements    |
| ------------------------- |:-------------:|
| Processes Created         | 5             |
| Threads Created           | 0             |
| Concurrent Server         | YES           |
| Three-way Handshake       | YES           |
| IO Type                   | Blocking      |
| Debug Mode                | NO            |
| Signal Handler            | NO            |
| Handles Malicious Clients | NO            |
| Handles Partial Writes    | NO            |
| Thread Pool               | NO            |
| Epoll                     | NO            |

## Usage
In order to compile the client run:

```
make client
```

In order to compile the server:

```
make server
```

Note: If make is not installed, you can install it using ```sudo apt-get install make``` or simply look inside the makefile and copy the gcc command.