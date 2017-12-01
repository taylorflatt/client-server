# Iterative client-server [![Build Status](https://travis-ci.org/taylorflatt/client-server.svg?branch=master)](https://travis-ci.org/taylorflatt/client-server)

## Background
Developed for Advanced C Programming (CS 591) in Fall 2017, each lab represents a step towards the "ultimate" client-server.

## Description
The functionality revolves around creating an ssh client-server which sets bash up for a connecting client on the server. However, this can easily be swapped for any other function; such as a web server (nginx). The labs crescendo into a much more sophisticated implementation using epoll and a thread pool to handle all processing. It also handles DDOS attacks and malicious clients.

Additionally, each folder has a makefile which can be used to make each client and server. More details for each particular implementation can be found in the particular folder:

- [Lab 1](https://github.com/taylorflatt/client-server/tree/master/Lab1)
- [Lab 2](https://github.com/taylorflatt/client-server/tree/master/Lab2)
- [Lab 3](https://github.com/taylorflatt/client-server/tree/master/Lab3)
- [Lab 4](https://github.com/taylorflatt/client-server/tree/master/Lab4)
- [Lab 5](https://github.com/taylorflatt/client-server/tree/master/Lab5)

The code was written with Linux in mind and doesn't necessarily conform to POSIX standards. Verify code before reuse.

## Features

| Feature                   | Lab 1         | Lab 2         | Lab 3         | Lab 4 (ThrPool) | Lab 5         |
| ------------------------- |:-------------:|:-------------:|:-------------:|:---------------:|:-------------:|
| Processes Created*        | 1             | 3             | 1             | -               | 1             |
| Threads Created           | 0             | 0             | 2             | -               | 8 (Num_Cores) |
| Concurrent Server         | YES           | YES           | YES           | -               | YES           |
| Three-way Handshake       | YES           | YES           | YES           | -               | YES           |
| IO Type                   | Blocking      | Blocking      | Blocking      | -               | Non-blocking  |
| Debug Mode                | NO            | YES           | YES           | YES             | YES           |
| Signal Handler            | NO            | YES           | YES           | NO              | YES           |
| Handles Malicious Clients | NO            | NO            | YES**         | -               | YES           |
| Handles Partial Writes    | NO            | NO            | NO            | -               | YES           |
| Thread Pool               | NO            | NO            | NO            | YES             | YES           |
| Epoll                     | NO            | NO            | YES           | -               | YES           |

*<sup><sub>It is important to note that there will be at least 1 process created for bash.</sub></sup>

**<sup><sub>Lab 3 handles malicious clients by using timers whereas Lab5 uses timerfd with its own epoll unit.</sub></sup>

## Usage

First start by running the server:

```
./server
```

Then connect a client (or more) using the server IP:

```
./client 127.0.0.1
```

## Auxiliary Scripts

The following scripts can be used to test or debug the server:

- [Broken Clients Test](https://github.com/taylorflatt/client-server/blob/master/Scripts/broken_clients_test.sh)
- [Concurrent Clients Test](https://github.com/taylorflatt/client-server/blob/master/Scripts/concurrent_clients_test.sh)
- [Flood Server Test](https://github.com/taylorflatt/client-server/blob/master/Scripts/flood_server_test.sh)
- [Flood Server Hybrid Test](https://github.com/taylorflatt/client-server/blob/master/Scripts/flood_server_hybrid_test.sh)
- [Pwrite](https://github.com/taylorflatt/client-server/blob/master/Scripts/pwrite.sh)
- [Show Server](https://github.com/taylorflatt/client-server/blob/master/Scripts/show-server)
- [Show Clients](https://github.com/taylorflatt/client-server/blob/master/Scripts/show-client)
- [Show Sockets](https://github.com/taylorflatt/client-server/blob/master/Scripts/show-server)
- [All Broken Clients](https://github.com/taylorflatt/client-server/tree/master/Scripts/BrokenClients)

## Future
I plan on implementing a logger to record and report what happens during server runtime such as:
- Server creation
- Client connection
- Client destruction
- Client errors
- Server errors