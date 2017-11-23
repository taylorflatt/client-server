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

/* Preprocessor constants. */
#define PORT 4070
#define MAX_LENGTH 4096
#define MAX_NUM_CLIENTS 64000
#define SECRET "cs407rembash\n"
#define CHALLENGE "<rembash>\n"
#define PROCEED "<ok>\n"
#define ERROR "<error>\n"

/* Global Variables */
int c_pid[2];   /* Stores the subprocess PIDs so they can be closed. */

/* Prototypes. */
int create_server();
void handle_client(int client_fd);
void create_processes(int client_fd);
pid_t open_pty(int *master_fd, int client_fd);
int transfer_data(int from, int to);
void sigchld_handler(int signal, siginfo_t *sip, void *ignore);
char *read_client_message(int client_fd);

int main(int argc, char *argv[]) {

    int client_fd, server_sockfd;
    socklen_t client_len;
    
    struct sockaddr_in client_address;

    if((server_sockfd = create_server()) == -1) {
        exit(EXIT_FAILURE);
    }

    if(signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        perror("(main) signal(): Failed setting SIGCHLD to SIG_IGN.");
        exit(EXIT_FAILURE);
    }

    // Main server loop
    while(1) {
        /* Collect any terminated children before attempting to accept a new connection. */
        while (waitpid(-1,NULL,WNOHANG) > 0);
        
        /* Accept a new connection and get a socket to use for client. */
        client_len = sizeof(client_address);
        if((client_fd = accept(server_sockfd, (struct sockaddr *) &client_address, &client_len)) == -1) {
            perror("(main) accept(): Failed accepting a client.");
        }
        
        /* Fork the client provided the fd is valid. */
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

/** Creates the server by setting up the socket and begins listening.
 * 
 * Returns: An integer corresponding to server's file descriptor on success or failure -1.
*/
int create_server() {
    int server_sockfd;
    struct sockaddr_in server_address;

    if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("(create_server) socket(): Error creating the socket.");
    }

    DTRACE("%ld:Starting server=%d.\n", (long)getpid(), server_sockfd);

    int i=1;
    if(setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i))) {
        perror("(create_server) setsockopt(): Error setting the server socket options.");
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT);

    if((bind(server_sockfd, (struct sockaddr *) &server_address, sizeof(server_address))) == -1){
        perror("(create_server) bind(): Error assigning an address to the socket.");
    }

    /* Start listening to server socket. */
    if((listen(server_sockfd, 10)) == -1){
        perror("(create_server) listen(): Error listening to the socket.");
    }

    return server_sockfd;
}

/** Delegator function which validates the client and creates the sub-processes for 
 * communication with the client.
 * 
 * Returns: None.
*/
void handle_client(int client_fd) {
    
    char *pass;

    DTRACE("%ld:Sending challenge to client=%d\n", (long)getpid(), client_fd);
    /* Send challenge to client. */
    write(client_fd, CHALLENGE, strlen(CHALLENGE));

    /* Receive the client's password. */
    if((pass = read_client_message(client_fd)) == NULL)
        return;
    
    /* Validate the client's response. */
    if(strcmp(pass, SECRET) == 0) {
        DTRACE("%ld:Client=%d has been validated.\n", (long)getpid(), client_fd);
        create_processes(client_fd);
        
    }  else {
        /* Invalid secret, tell the client. */
        write(client_fd, ERROR, strlen(ERROR));
        perror("(handle_client) write(): Invalid secret, aborting connection!");
    }
}

/** Function performs the following duties:
 *      - Creates a signal handler for the client.
 *      - Creates a bash pty for the client.
 *      - Creates a subprocess which relays data from the client to the pty.
 *      - Creates a subprocess which relays data from the pty to the client.
 *      - Collects processes after successful completion.
 * 
 * client_fd: File descriptor for a client process.
 * 
 * Returns: None.
 */
void create_processes(int client_fd) {
    
    int master_fd;

    struct sigaction act;
    act.sa_sigaction = sigchld_handler;
    act.sa_flags = SA_SIGINFO|SA_RESETHAND;  /* SA_SIGINFO required to use .sa_sigaction instead of .sa_handler. */
    sigemptyset(&act.sa_mask);

    if(sigaction(SIGCHLD, &act, NULL) == -1) {
        perror("(create_processes) sigaction(): Error creating the signal handler.");
        exit(EXIT_FAILURE);
    }

    if((c_pid[0] = open_pty(&master_fd, client_fd)) == -1) {
        perror("(create_processes) open_pty(): Error setting up the pty for the client.");
        exit(EXIT_FAILURE);
    }

    if(sigaction(SIGCHLD, &act, NULL) == -1) {
        perror("(create_processes) sigaction(): Error setting signal for SIGCHLD.");
        return;
    }
    
    /* Reads from the client (socket) and writes to the master. */
    if((c_pid[1] = fork()) == 0) {

        DTRACE("%ld:New subprocess for data transfer socket-->PTY: PID=%ld, PGID=%ld, SID=%ld\n", (long)getppid(), (long)getpid(), (long)getpgrp(), (long)getsid(0));
        DTRACE("%ld:Starting data transfer socket-->PTY (FD %d-->%d)\n",(long)getpid(), client_fd, master_fd);
        transfer_data(client_fd, master_fd);
        DTRACE("%ld:Completed data transfer socket-->PTY, so terminating...\n", (long)getpid());

        exit(EXIT_SUCCESS);
    }

    /* Let the client know things are ready to proceed. */
    write(client_fd, PROCEED, strlen(PROCEED));

    /* Read from the pty and write to the client. */
    DTRACE("%ld:Starting data transfer PTY-->socket (FD %d-->%d)\n",(long)getpid(), master_fd, client_fd);
    transfer_data(master_fd, client_fd);
    DTRACE("%ld:Completed data transfer PTY-->socket\n",(long)getpid());

    /* Ignore the sig handler and kill the children. */
    act.sa_handler = SIG_IGN;
    kill(c_pid[0], SIGTERM);
    kill(c_pid[1], SIGTERM);

    /* Collect any remaining child processes for the client. */
    while (waitpid(-1,NULL,WNOHANG) > 0);
        
    return;
}

/** Opens the PTY and creates the connection between bash and the client by forking off a subprocess 
 * to run bash.
 * 
 * master_fd: A pointer to the master file descriptor for a client.
 * client_fd: An integer corresponding to the client's file descriptor.
 * 
 * Returns: A pid_t representing the PID of the forked child process.
*/
pid_t open_pty(int *master_fd, int client_fd) {
    
    char * slavename;
    int pty_master, slave_fd, err;

    /* Open an unused pty dev and store the fd for later reference.
        O_RDWR := Open pty for reading + writing. 
        O_NOCTTY := Don't make it a controlling terminal for the process. 
    */
    if((pty_master = posix_openpt(O_RDWR|O_NOCTTY)) == -1) {
        return -1;
    }
    
    /* Attempt to kickstart the master.
        Grantpt := Creates a child process that executes a set-user-ID-root program changing ownership of the slave 
            to be the same as the effective user ID of the calling process and changes permissions so the owner 
            has R/W permissions.
        Unlockpt := Removes the internal lock placed on the slave corresponding to the pty. (This must be after grantpt).
        ptsname := Returns the name of the pty slave corresponding to the pty master referred to by pty_master. (/dev/pts/nn).
    */
    if(grantpt(pty_master) == -1 || unlockpt(pty_master) == -1 || (slavename = ptsname(pty_master)) == NULL) {
        err = errno;
        close(pty_master);
        errno = err;
        return -1;
    }
    
    // Child
    pid_t c_pid;
    if((c_pid = fork()) == 0) {

        /* pty_master is not needed in the child, close it. */
        close(pty_master);
        close(client_fd);

        if(setsid() == -1) {
            perror("(open_pty) setsid(): Error creating a new session.");
            return -1;            
        }

        if((slave_fd = open(slavename, O_RDWR)) == -1) {
            perror("(open_pty) open(): Error opening the slave_fd for RW IO.");
            return -1;
        }

        DTRACE("%ld:PTY_MASTER=%i and PTY_SLAVE=%d.\n", (long)getppid(), pty_master, slave_fd);  

        if ((dup2(slave_fd, STDIN_FILENO) == -1) || (dup2(slave_fd, STDOUT_FILENO) == -1) || (dup2(slave_fd, STDERR_FILENO) == -1)) {
            perror("(open_pty) dup2(): Error redirecting in/out/error.");
            exit(EXIT_FAILURE); 
        }

        /* No longer needed and don't want it open within bash. */
        close(slave_fd);

        execlp("bash", "bash", NULL);

        /* Should never reach this point. */
        exit(EXIT_FAILURE);
    }

    /* Set the master pty correctly. */
    *master_fd = pty_master;
    return c_pid;
}

/** Transfers data between two file descriptors.
 * 
 * from: Integer representing the source file descriptor (read).
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
int transfer_data(int from, int to)
{
    char buf[MAX_LENGTH];
    ssize_t nread, nwrite;

    while ((nread = read(from, buf, MAX_LENGTH)) > 0) {
        if ((nwrite = write(to, buf, nread)) == -1) {
            break;
        }
    }

    if (nread == -1) {
        DTRACE("%ld:Error read()'ing from FD %d\n",(long)getpid(), from);
    } 
    if (nwrite == -1) {
        DTRACE("%ld:Error write()'ing to FD %d\n",(long)getpid(), to);
    }

    if (errno) {
        return -1;
    }

    return 0;
}

// Collects processes.
/** Catches signals and closes the client subprocesses out nicely collecting children.
 * 
 * signal: The signal that was sent.
 * sip: A pointer which contains signal information such as the PID.
 * ignore: Unused.
 * 
 * Returns: None.
 */
void sigchld_handler(int signal, siginfo_t *sip, void *ignore) {

    /* Terminate the process which didn't create the signal. */
    if (sip->si_pid == c_pid[0]) {
        DTRACE("%ld:SIGCHLD handler invoked due to PID:%d (bash), killing %d\n", (long)getpid(), sip->si_pid, c_pid[1]);
        kill(c_pid[1], SIGTERM); 
    } else {
        DTRACE("%ld:SIGCHLD handler invoked due to PID:%d (socket-->PTY), killing %d\n", (long)getpid(), sip->si_pid, c_pid[0]);
        kill(c_pid[0], SIGTERM); 
    }

    /* Collect all children for the client. */
    while (waitpid(-1, NULL, WNOHANG) > 0);

    /* Terminate the parent client. */
    DTRACE("%ld:Terminating (client)...\n", (long)getpid());
    _exit(EXIT_SUCCESS);
}

/** Reads the handshake messages.
 * 
 * client_fd: An integer corresponding to the clients's file descriptor.
 * 
 * Returns: A null terminated string if successful, otherwise NULL if an error is
 *          encountered.
*/
char *read_client_message(int client_fd)
{
  static char msg[MAX_LENGTH];
  int nread;
  
    if ((nread = read(client_fd, msg, MAX_LENGTH - 1)) <= 0) {
        if (errno) {
            perror("(read_client_message) read(): Error reading.");
        } else {
            perror("(read_client_message) read(): The client connection closed unexpectedly.");
        }
            
        return NULL; 
    }

  msg[nread] = '\0';

  return msg;
}
