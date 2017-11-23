#define _XOPEN_SOURCE 700
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/tcp.h>
#include <netinet/in.h>
#include "DTRACE.h"

//Preprocessor constants
#define PORT 4070
#define MAX_LENGTH 4096
#define MAX_NUM_CLIENTS 64000
#define SECRET "cs407rembash\n"
#define CHALLENGE "<rembash>\n"
#define PROCEED "<ok>\n"
#define ERROR "<error>\n"

int c_pid[5];

int client_fd_tuples[MAX_NUM_CLIENTS * 2 + 5];
pid_t bash_fd[MAX_NUM_CLIENTS * 2 + 5];

//Prototypes
void handle_client(int client_fd);
void create_processes(int client_fd);
pid_t pty_open(int *master_fd, int client_fd);
int transfer_data(int from, int to);
void sigchld_handler(int signal);
char *read_client_message(int client_fd);
int create_server();

int create_server() {
    int server_sockfd;
    struct sockaddr_in server_address;

    if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error creating socket, error: %s\n", strerror(errno));
    }

    int i=1;
    if(setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i))) {
        fprintf(stderr, "Error setting sockopt.");
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT);

    if((bind(server_sockfd, (struct sockaddr *) &server_address, sizeof(server_address))) == -1){
        fprintf(stderr, "Error assigning address to socket, error: %s\n", strerror(errno));
    }

    // Start listening to server socket.
    if((listen(server_sockfd, 10)) == -1){
        fprintf(stderr, "Error listening to socket, error: %s\n", strerror(errno));
    }

    return server_sockfd;
}

int main(int argc, char *argv[]) {

    int client_fd, server_sockfd;
    socklen_t client_len;
    
    struct sockaddr_in client_address;

    if((server_sockfd = create_server()) == -1) {
        exit(EXIT_FAILURE);
    }

    if(signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        fprintf(stderr, "Error setting SIGCHLD to SIG_IGN.");
        exit(EXIT_FAILURE);
    }

    // Main server loop
    while(1) {
        // Collect any terminated children before attempting to accept a new connection.
        while (waitpid(-1,NULL,WNOHANG) > 0);
        
        // Accept a new connection and get socket to use for client:
        client_len = sizeof(client_address);
        if((client_fd = accept(server_sockfd, (struct sockaddr *) &client_address, &client_len)) == -1) {
            fprintf(stderr, "Error making connection, error: %s\n", strerror(errno));
        }
        
        // Fork immediately.
        if(client_fd != -1) {
            if(fork() == 0) {
                close(server_sockfd);
                handle_client(client_fd);
            }
            close(client_fd);
        }
    }

    exit(EXIT_SUCCESS);
}

// Handles the three-way handshake and starts up the create_processes method.
void handle_client(int client_fd) {
    
    char *pass;

    // Send challenge to client.
    printf("Sending challenge to client.\n");

    write(client_fd, CHALLENGE, strlen(CHALLENGE));

    // TIMER FOR SIGNAL HANDLER
    // Read password from client.
    if((pass = read_client_message(client_fd)) == NULL)
        return;
    // STOP TIMER.
    
    // Make sure the password is good.
    if(strcmp(pass, SECRET) == 0) {
        printf("Challenge passed. Moving into accept client.\n");

        // let client know shell is ready by sending <ok>\n
        write(client_fd, PROCEED, strlen(PROCEED));
        create_processes(client_fd);
        
    }  else {
        // Invalid secret, tell the client.
        write(client_fd, ERROR, strlen(ERROR));
        fprintf(stderr, "Aborting connection. Invalid secret: %s", pass);
    }
}

// Creates two processes which read and write on a socket connecting the client and server.
void create_processes(int client_fd) {
    
    int master_fd;

    struct sigaction act;
    act.sa_handler = sigchld_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    if(sigaction(SIGCHLD, &act, NULL) == -1) {
        perror("Creation of the signal handler failed!");
        exit(1);
    }

    printf("Setting c_pid[0] to pty_open.\n");

    if((c_pid[0] = pty_open(&master_fd, client_fd)) == -1) 
        exit(1);
    
    // Reads from the client (socket) and writes to the master.
    pid_t pid;
    if((pid = fork()) == 0) {

        DTRACE("%ld:New subprocess for data transfer socket-->PTY: PID=%ld, PGID=%ld, SID=%ld\n", (long)getppid(), (long)getpid(), (long)getpgrp(), (long)getsid(0));
        DTRACE("%ld:Starting data transfer socket-->PTY (FD %d-->%d)\n",(long)getpid(), client_fd, master_fd);
        transfer_data(client_fd, master_fd);
        DTRACE("%ld:Completed data transfer socket-->PTY, so terminating...\n", (long)getpid());

        exit(EXIT_SUCCESS);
    }

    c_pid[1] = pid;
    printf("PID of child socketreader: %d\n", (int)pid);

    //Loop, reading from PTY master (i.e., bash) and writing to client socket:
    DTRACE("%ld:Starting data transfer PTY-->socket (FD %d-->%d)\n",(long)getpid(), master_fd, client_fd);
    //int transfer_status = transfer_data(ptymaster_fd,client_fd);
    transfer_data(master_fd, client_fd);
    DTRACE("%ld:Completed data transfer PTY-->socket\n",(long)getpid());

    close(client_fd);
    close(master_fd);

    act.sa_handler = SIG_IGN;

    if(sigaction(SIGCHLD, &act, NULL) == -1)
        perror("Client: Error setting SIGCHLD");
    return;
}

// Creates the master and slave pty.
pid_t pty_open(int *master_fd, int client_fd) {
    
    char * slavename;
    int pty_master, slave_fd, err;

    /* Open an unused pty dev and store the fd for later reference.
        O_RDWR := Open pty for reading + writing. 
        O_NOCTTY := Don't make it a controlling terminal for the process. 
    */
    if((pty_master = posix_openpt(O_RDWR|O_NOCTTY)) == -1)
        return -1;
    
    /* Attempt to kickstart the master.
        Grantpt := Creates a child process that executes a set-user-ID-root program changing ownership of the slave 
            to be the same as the effective user ID of the calling process and changes permissions so the owner 
            has R/W permissions.
        Unlockpt := Removes the internal lock placed on the slave corresponding to the pty. (This must be after grantpt).
        ptsname := Returns the name of the pty slave corresponding to the pty master referred to by pty_master. (/dev/pts/nn).
    */
    printf("Before granting pt\n");
    if(grantpt(pty_master) == -1 || unlockpt(pty_master) == -1 || (slavename = ptsname(pty_master)) == NULL) {
        err = errno;
        close(pty_master);
        errno = err;
        return -1;
    }
    
    // Child
    pid_t c_pid;
    if((c_pid = fork()) == 0) {
        printf("slavename = %s\n", slavename);
        
        // pty_master is not needed in the child, close it.
        close(pty_master);
        close(client_fd);

        if(setsid() == -1) {
            printf("Could not create a new session.\n");
            return -1;            
        }

        if((slave_fd = open(slavename, O_RDWR)) == -1) {
            printf("Could not open %s\n", slavename);
            return -1;
        }

        printf("Setting dup\n");
        if ((dup2(slave_fd, STDIN_FILENO) == -1) || (dup2(slave_fd, STDOUT_FILENO) == -1) || (dup2(slave_fd, STDERR_FILENO) == -1)) {
            perror("dup2() call for FD 0, 1, or 2 failed");
            exit(EXIT_FAILURE); 
        }

        /* No longer needed and don't want it open within bash. */
        close(slave_fd);

        execlp("bash", "bash", NULL);

        /* Should never reach this point. */
        exit(EXIT_FAILURE);
    }

    printf("slave pid = %d\n", (int)c_pid);

    * master_fd = pty_master;
    return c_pid;
}

int transfer_data(int from, int to)
{
    char buf[MAX_LENGTH];
    ssize_t nread, nwrite;

    while ((nread = read(from, buf, MAX_LENGTH)) > 0) {
    if ((nwrite = write(to, buf, nread)) == -1) break;
    }

    #ifdef DEBUG
    if (nread == -1) DTRACE("%ld:Error read()'ing from FD %d\n",(long)getpid(), from);
    if (nwrite == -1) DTRACE("%ld:Error write()'ing to FD %d\n",(long)getpid(), to);
    #endif

    //In case of an error, return false/fail:
    if (errno) return 0;

    //Normal true/success return:
    return 1;
}

// Collects processes.
void sigchld_handler(int sig) {
    wait(NULL);

    kill(c_pid[0], sig);
    kill(c_pid[1], sig);
    exit(0);
}

// Reads a messagr from a server and returns it as a string (null terminated).
// Also handles read errors internally returning NULL if an error is encountered.
char *read_client_message(int client_fd)
{
  static char msg[MAX_LENGTH];
  int nread;
  
    if ((nread = read(client_fd, msg, MAX_LENGTH - 1)) <= 0) {
        if (errno)
            perror("Error reading from the client socket");
        else
            fprintf(stderr, "Client closed connection unexpectedly\n");
            
        return NULL; 
    }

  msg[nread] = '\0';

  return msg;
}
