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

// Preprocessor Constants
#define MAX_LENGTH 4096
#define PORT 4070
#define SECRET "cs407rembash\n"
#define CHALLENGE "<rembash>\n"
#define PROCEED "<ok>\n"

//Prototypes
int connect_server(const char *server_ip);
int handshake(int server_fd);
char *read_handshake_messages(int client_fd);
int set_tty_noncanon_noecho();
int create_child_handler_signal();
void communicate_with_server(int server_fd);
int transfer_data(int from, int to);
void sigchld_handler(int signal);
void graceful_exit();
void restore_tty_settings();

struct termios saved_tty_settings;

pid_t cpid;

int main(int argc, char *argv[]){
    int server_fd;
    char ip[MAX_LENGTH];

    // Check command-line arguments and store ip.
    if(argc != 2) {
        fprintf(stderr, "\nIncorrect number of command line arguments!\n\n");
        printf("    Usage: %s SERVER_IP\n\n", argv[0]);
        exit(EXIT_FAILURE);
    } else {
        strcpy(ip, argv[1]);
    }

    DTRACE("Client starting: PID=%ld, PGID=%ld, SID=%ld\n", (long)getpid(), (long)getpgrp(), (long)getsid(0));

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

    /* Normal termination. */
    DTRACE("%ld:Client termination...\n", (long)getpid());
    if (errno) {
        graceful_exit(EXIT_FAILURE);
    }

    graceful_exit(EXIT_SUCCESS);

    return 0;
}

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
    DTRACE("%ld:Setting terminal to non-canon mode.\n", (long)getpid());
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
    
    DTRACE("%ld:Creating child termination signal handler.\n", (long)getpid());
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

void sigchld_handler(int signal) {

    DTRACE("%ld:Caught signal from subprocess termination...terminating!\n", (long)getpid());
    graceful_exit();
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

        DTRACE("%ld:Starting data transfer stdin-->socket (FD 0-->%d)\n", (long)getpid(), server_fd);
        if(transfer_data(STDIN_FILENO, server_fd) == -1) {
            perror("(communicate_with_server) transfer_data(): Error reading from stdin. ");
            graceful_exit(EXIT_FAILURE);
        }
        DTRACE("%ld:Completed data transfer stdin-->socket\n", (long)getpid());

        graceful_exit(EXIT_SUCCESS);
    }

    /* PARENT PROCESS */
    DTRACE("%ld:Starting data transfer socket-->stdout (FD %d-->1)\n", (long)getpid(), server_fd);
    if(transfer_data(server_fd, STDOUT_FILENO) == -1) {
        perror("(communicate_with_server) transfer_data(): Error reading from the server and writing to STDOUT.");
    }
    DTRACE("%ld:Completed data transfer socket-->stdout\n", (long)getpid());

    DTRACE("%ld:Normal transfer completion, terminating child (%ld)\n", (long)getpid(), (long)cpid);
    signal(SIGCHLD, SIG_IGN);
    kill(cpid, SIGTERM);

    return;
}

int transfer_data(int from, int to)
{
    char buff[4096];
    ssize_t nread;

    while ((nread = read(from, buff, MAX_LENGTH)) > 0) {
        if (write(to, buff, nread) == -1) break;
    }

    if (nread == 0)
    DTRACE("%ld:EOF on FD %d!\n", (long)getpid(), from);

    //In case of an error, return false/fail:
    if (errno) return 0;

    //Normal true/success return:
    return 1;
}

void graceful_exit() {

    restore_tty_settings();

    int childstatus;
    wait(&childstatus);

    //Determine if exit status should be failure:
    if (!WIFEXITED(childstatus) || WEXITSTATUS(childstatus) != EXIT_SUCCESS) {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

void restore_tty_settings() {

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty_settings) == -1) {
        perror("Restoring TTY attributes failed");
        exit(EXIT_FAILURE); 
    }

    return;
}
