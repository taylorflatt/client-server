/** Lab3: Client 10/16/2017 for CS 591.
 * 
 * Author: Taylor Flatt
 * 
 * Properties:
 *   -- uses plain read() with initial protocol exchange.
 *   -- uses two subprocesses for commands-shell exchange:
 *           (parent) read from socket and write to stdout.
 *           (child)  read from stdin and write to socket.
 *   -- uses read()/write() for all I/O.
 *   -- assumes all write()'s are done in full (no partials), which is true since blocking mode.
 *   -- setups up SIGCHLD handler to deal with premature child process termination.
 *   -- parent always collects child before termination.
 *   -- TTY changed to noncanonical mode.
 *  -- TTY reset to canonical mode before termination.
 * 
 * Usage: client SERVER_IP_ADDRESS
*/

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <netinet/in.h>
#include <signal.h>
#include "DTRACE.h"

/* Preprocessor Constants */
#define MAX_LENGTH 4096
#define PORT 4070
#define SECRET "cs407rembash\n"
#define CHALLENGE "<rembash>\n"
#define PROCEED "<ok>\n"

/* Prototypes. */
int connect_server(const char *server_ip);
int handshake(int server_fd);
char *read_handshake_messages(int server_fd);
int set_tty_noncanon_noecho();
int create_child_handler_signal();
void sigchld_handler(int signal);
void communicate_with_server(int server_fd);
int transfer_data(int from, int to);
void graceful_exit(int exit_status);
void restore_tty_settings();

/* Global Variables. */
struct termios saved_tty_settings;

int main(int argc, char *argv[]){
    int server_fd;
    char ip[MAX_LENGTH];
    
    /* Make sure the command-line arguments are in order. */
    if(argc != 2) {
        fprintf(stderr, "\nIncorrect number of command line arguments!\n\n");
        printf("    Usage: %s SERVER_IP\n\n", argv[0]);
        exit(EXIT_FAILURE);
    } else {
        strcpy(ip, argv[1]);
    }

    DTRACE("Client starting: PID=%ld, PGID=%ld, SID=%ld\n",(long)getpid(),(long)getpgrp(),(long)getsid(0));

    if((server_fd = connect_server(ip)) == -1) {
        perror("(main) connect_server(): Failed to connect to server.");
        exit(EXIT_FAILURE);
    }

    /* Set SIGPIPE to ignored so that a write to a closed 
     * connection causes an error return rather than a SIGPIPE termination. 
     */
    signal(SIGPIPE, SIG_IGN);
    
    if(handshake(server_fd) == -1) {
        perror("(main) handshake(): Failed handshake with the server.");
        exit(EXIT_FAILURE);
    }

    if(set_tty_noncanon_noecho() == -1) {
        perror("(main) set_tty_noncanon_noecho(): Failed setting client's terminal settings.");
        exit(EXIT_FAILURE);
    }

    /* Make sure our children don't become brain sucking zombies. */
    if(create_child_handler_signal() == -1) {
        perror("(main) create_child_handler_signal(): Error creating signal to handle zombie children.");
        graceful_exit(EXIT_FAILURE);
    }

    communicate_with_server(server_fd);

    // Normal termination.
    DTRACE("%ld:Client termination...\n",(long)getpid());
    if (errno)
        graceful_exit(EXIT_FAILURE);

    graceful_exit(EXIT_SUCCESS);

    return 0;
}

/** Connects the client to a server with a particular IP/Port.
 * 
 * server_ip: A string corresponding to the server's IP.
 * 
 * Returns: An integer corresponding to the server's file descriptor if successful. Otherwise, 
 *          it returns a -1.
*/
int connect_server(const char *server_ip) {
    int server_fd;
    struct sockaddr_in serv_address;

    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("(connect_server) socket(): Failed setting the server's socket.");
        return -1;
    }

    /* Name the socket and set the port. */
    serv_address.sin_family = AF_INET;
    serv_address.sin_port = htons(PORT);
    inet_aton(server_ip, &serv_address.sin_addr);

    /* Connect the client and server. */
    if((connect(server_fd, (struct sockaddr*)&serv_address, sizeof(serv_address))) == -1) {
        perror("(connect_server) connect(): Failed connecting to the server.");
        return -1;
    }

    return server_fd;
}

/** Conducts a three-way handshake with the server to verify that the client is 
 * authorized to connect.
 * 
 * server_fd: An integer corresponding to the server's file descriptor.
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
int handshake(int server_fd) {
    
    char *h_msg;

    /* Receive the challenge. */
    if((h_msg = read_handshake_messages(server_fd)) == NULL) {
        perror("(handshake) read_handshake_messages(): Failed reading the challenge from the server.");
        return -1;
    }

    /* Verify the challenge against our known result. */
    if(strcmp(h_msg, CHALLENGE) != 0) {
        perror("(handshake) strcmp(): Incorrect challenge received from the server");
        return -1;
    }

    /* Send SECRET to the server for verification. */
    if((write(server_fd, SECRET, strlen(SECRET))) == -1) {
        perror("(handshake) write(): Failed sending the secret to the server.");
        return -1;
    }

    /* Receive the server's verification message. */
    if((h_msg = read_handshake_messages(server_fd)) == NULL) {
        perror("(handshake) read_handshake_messages(): Failed to receive the server's PROCEED message.");
        return -1;
    }

    /* Verify the server's response to our known result. */
    if(strcmp(h_msg, PROCEED) != 0) {
        perror("(handshake) strcmp(): The server's PROCEED message is invalid.");
        return -1;
    }

    return 0;
}

/** Reads the handshake messages.
 * 
 * client_fd: An integer corresponding to the clients's file descriptor.
 * 
 * Returns: A null terminated string if successful, otherwise NULL if an error is
 *          encountered.
*/
char *read_handshake_messages(int client_fd)
{
  static char msg[MAX_LENGTH];
  int nread;
  
    if ((nread = read(client_fd, msg, MAX_LENGTH - 1)) <= 0) {
        if (errno)
            perror("(read_handshake_messages) read(): Error reading from the client socket.");
        else
            perror("(read_handshake_messages) read(): Client closed connection unexpectedly.");
            
        return NULL; 
    }

  msg[nread] = '\0';

  return msg;
}

/** Sets the bash terminal to non-canonical and non-echo mode.
 * 
 * Remarks: This is important so that signals aren't displayed to the 
 *          client when they hit backspace and for programs such as 
 *          top/vi to work.
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
int set_tty_noncanon_noecho()
{
    DTRACE("%ld:Setting terminal to non-canon mode.\n",(long)getpid());
    struct termios tty_settings;

    // Get the current terminal settings.
    if (tcgetattr(STDIN_FILENO, &tty_settings) == -1) {
        perror("(set_tty_noncanon_noecho) tcgetattr(): Getting TTY attributes failed");
        return -1;
    }

    /* Save terminal settings so they can be restored later. */
    saved_tty_settings = tty_settings;

    /* Set terminal settings appropriately. */
    tty_settings.c_lflag &= ~(ICANON | ECHO);   /* Sets terminal to non-canonical mode. */
    tty_settings.c_lflag |= ISIG;               /* Does not echo back input (exit exit). */
    tty_settings.c_lflag &= ~ICRNL;             /* Allows signals to be generated when detected. */
    tty_settings.c_cc[VMIN] = 1;                /* Minimum number of characters for noncanonical reads. */
    tty_settings.c_cc[VTIME] = 0;               /* Timeout in deciseconds for noncanonical reads. */

    /* Put terminal in raw mode after flushing. */
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tty_settings) == -1) {
        perror("(set_tty_noncanon_noecho) tcsetattr(): Setting TTY attributes failed");
        return -1;
    }

    return 0;
}

/** Creates a signal handler to detect when a subprocess exits.
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
int create_child_handler_signal() {
    
    DTRACE("%ld:Creating child termination signal handler.\n",(long)getpid());
    struct sigaction act;
    act.sa_handler = &sigchld_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    if(sigaction(SIGCHLD, &act, NULL) == -1) {
        perror("(create_child_handler_signal) sigaction(): Creation of the signal handler failed!");
        return -1;
    }

    return 0;
}

/** Associated with create_child_handler_signal and will gracefully shut the
 * client down.
 * 
 * Returns: None.
*/
void sigchld_handler(int sig) {
    
    DTRACE("%ld:Caught signal from subprocess termination...terminating!\n",(long)getpid());
    graceful_exit(EXIT_SUCCESS);
}

/** Handles the communication to and from the server by creating a subprocess which handles 
 * communication from stdin to the server. The parent process handles communication coming 
 * from the server to the client and displays it appropriately.
 * 
 * Child: Sends communication from stdin -> socket.
 * Parent: Sends communication from socket -> stdout.
 * 
 * Returns: None.
*/
void communicate_with_server(int server_fd) {

    pid_t cpid;

    /* CHILD PROCESS */
    if((cpid = fork()) == 0) {
        cpid = getppid();   /* Reuse since the child's pid isn't required here. */

        DTRACE("%ld:Starting data transfer stdin-->socket (FD 0-->%d)\n",(long)getpid(),server_fd);
        if(transfer_data(STDIN_FILENO, server_fd) == -1) {
            perror("(communicate_with_server) transfer_data(): Error reading from stdin. ");
            graceful_exit(EXIT_FAILURE);
        }
        DTRACE("%ld:Completed data transfer stdin-->socket\n",(long)getpid());

        graceful_exit(EXIT_SUCCESS);
    }

    /* PARENT PROCESS */
    DTRACE("%ld:Starting data transfer socket-->stdout (FD %d-->1)\n",(long)getpid(),server_fd);
    if(transfer_data(server_fd, STDOUT_FILENO) == -1) {
        perror("(communicate_with_server) transfer_data(): Error reading from the server and writing to STDOUT.");
    }
    DTRACE("%ld:Completed data transfer socket-->stdout\n",(long)getpid());

    DTRACE("%ld:Normal transfer completion, terminating child (%ld)\n",(long)getpid(),(long)cpid);
    signal(SIGCHLD, SIG_IGN);
    kill(cpid, SIGTERM);

    return;
}

/** Actually reads and writes data to and from sockets.
 * 
 * from: Integer representing the source file descriptor (read).
 * to: Integer representing the targer file descriptor (write).
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
int transfer_data(int from, int to) {
    char buf[MAX_LENGTH];
    ssize_t nread;

    while((nread = read(from, buf, MAX_LENGTH)) > 0) {
        if(write(to, buf, nread) == -1) {
            fprintf(stderr, "Failed writing data.");
            break;
        }
    }

    if(nread == -1 && errno) {
        fprintf(stderr, "Failed reading data.");
        return -1;
    }

    return 0;
}

/** Called in order to terminate in a sane way. Function restores the 
 * tty settings, collects any children processes, and determines the 
 * appropriate exit status to return based on the children.
 * 
 * Remarks: Note that even if the request exit status is EXIT_SUCCESS, 
 * an EXIT_FAILURE may still be used if the children have problems dying.
 * 
 * exit_status: An integer corresponding to the requested exit status.
 * 
 * Returns: None.
*/
void graceful_exit(int exit_status)
{
    int childstatus;
    DTRACE("%ld:Started exit procedure.\n",(long)getpid());
    restore_tty_settings();

    /* Collect any children and get their exit statuses. */
    DTRACE("%ld:Cleaning up children.\n",(long)getpid());
    wait(&childstatus);

    if(!WIFEXITED(childstatus) || WEXITSTATUS(childstatus) != EXIT_SUCCESS) {
        DTRACE("%ld:Error cleaning up children.\n",(long)getpid());
        exit(EXIT_FAILURE);
    }

    if (exit_status == EXIT_FAILURE)
        exit(EXIT_FAILURE);

    DTRACE("%ld:Successfully cleaned up children. Exiting...\n",(long)getpid());

    exit(EXIT_SUCCESS);
}

/** Restores the initial saved tty settings prior to making changes.
 * 
 * Remarks: This requires that the settings were saved in a global variable.
 * 
 * Returns: None.
*/
void restore_tty_settings()
{
    DTRACE("%ld:Restoring TTY settings.\n",(long)getpid());

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty_settings) == -1) {
        DTRACE("%ld:Failed restoring TTY settings.\n",(long)getpid());
        perror("(restore_tty_settings) tcsetattr(): Restoring TTY attributes failed");
        exit(EXIT_FAILURE);
    }

    return;
}