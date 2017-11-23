
# Lab 5
Completed on November 27th.

## Description
A sophisticated concurrent client-server which uses a thread pool and epoll to handle client connection and communication. The server also uses timerfds as a way to handle tardy clients who take too long to connect.

## Features

| Feature                   | Implements    |
| ------------------------- |:-------------:|
| Processes Created         | 1             |
| Threads Created           | 8 (Num_Cores) |
| Concurrent Server         | YES           |
| Three-way Handshake       | YES           |
| IO Type                   | Non-blocking  |
| Debug Mode                | YES           |
| Signal Handler            | YES           |
| Handles Malicious Clients | YES           |
| Handles Partial Writes    | YES           |
| Thread Pool               | YES           |
| Epoll                     | YES           |

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

In order to run or test with malfunctioning/malicious clients, run the makefile in the [Broken Clients](https://github.com/taylorflatt/client-server/tree/master/Lab5/BrokenClients) directory.

```
make allclients
```

Similarly for the debugging option:

```
make allclientsD
```

Note: If make is not installed, you can install it using ```sudo apt-get install make``` or simply look inside the makefile and copy the gcc command.