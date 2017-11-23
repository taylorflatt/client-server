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
void close_socket(int connect_fd);
char *read_server_message(int server_fd);
int create_tty(int fd, struct termios *prev_pty);
int transfer_data(int from, int to);
void sigchld_handler(int signal);
void stop(int socket, int exit_status);

struct termios saved_tty_settings;

pid_t cpid;

int main(int argc, char *argv[]){
    int server_fd;
    struct sockaddr_in serv_address;
    char ip[MAX_LENGTH];
    char *handshake;

    // Check command-line arguments and store ip.
    if(argc != 2) {
        fprintf(stderr, "\nIncorrect number of command line arguments!\n\n");
        printf("    Usage: %s SERVER_IP\n\n", argv[0]);
        exit(EXIT_FAILURE);
    } else {
        strcpy(ip, argv[1]);
    }

    // Create client socket.
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error creating socket, error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Name the socket, as agreed with the server.
    serv_address.sin_family = AF_INET;
    serv_address.sin_port = htons(PORT);
    inet_aton(ip, &serv_address.sin_addr);

    // Connect the client and server sockets.
    if((connect(server_fd, (struct sockaddr*)&serv_address, sizeof(serv_address))) == -1) {
        fprintf(stderr, "Error connecting to server, error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Receive the challenge.
    if((handshake = read_server_message(server_fd)) == NULL) {
        exit(EXIT_FAILURE);
    }

    //  Verify the challenge against known result.
    if(strcmp(handshake, CHALLENGE) != 0) {
        fprintf(stderr, "Error receving challenge from server, error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Send secret to the server for verification.
    if((write(server_fd, SECRET, strlen(SECRET))) == -1) {
        fprintf(stderr, "Error sending secret to server, error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Receive server verification.
    if((handshake = read_server_message(server_fd)) == NULL) {
        exit(EXIT_FAILURE);
    }

    // Receive the final verification from the server to proceed.
    if(strcmp(handshake, PROCEED) != 0) {
        printf("%s", handshake);
        exit(EXIT_FAILURE);    
    }

    // Signal handler. Make sure our children don't become brain sucking zombies.
    struct sigaction act;
    act.sa_handler = sigchld_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    if(sigaction(SIGCHLD, &act, NULL) == -1) {
        perror("Creation of the signal handler failed!");
        exit(1);
    }

    // Setup the terminal for the client.
    struct termios prev_pty;
    if(create_tty(1, &prev_pty) == -1) {
        tcsetattr(1, TCSAFLUSH, &prev_pty);
        close(server_fd);
        perror("Tcsetattr failed.");
    }

    // TODO: Swap over to the server method of writing so I'm not using memset.
    // Create a child process.
    if((cpid = fork()) == 0) {

        DTRACE("%ld:Starting data transfer stdin-->socket (FD 0-->%d)\n",(long)getpid(),server_fd);
        int transfer_status = transfer_data(STDOUT_FILENO, server_fd);
        DTRACE("%ld:Completed data transfer stdin-->socket\n",(long)getpid());

        if(!transfer_status) {
            perror("Error reading");
            exit(EXIT_FAILURE);
        }

        exit(EXIT_SUCCESS);
    }

    // Parent

    DTRACE("%ld:Starting data transfer socket-->stdout (FD %d-->1)\n",(long)getpid(),server_fd);
    int transfer_status = transfer_data(server_fd, STDIN_FILENO);
    DTRACE("%ld:Completed data transfer socket-->stdout\n",(long)getpid());

    if(!transfer_status) {
        perror("Error reading");
    }

    /* Eliminate SIGCHLD handler and kill child. */
    DTRACE("%ld:Normal transfer completion, terminating child (%ld)\n",(long)getpid(),(long)cpid);
    signal(SIGCHLD, SIG_IGN);
    kill(cpid, SIGTERM);

    // Make sure to close the connection upon exiting.
    close_socket(server_fd);
    tcsetattr(1, TCSAFLUSH, &prev_pty);
    act.sa_handler = SIG_IGN;

    if(sigaction(SIGCHLD, &act, NULL) == -1)
        perror("Client: Error setting SIGCHLD");

    exit(EXIT_SUCCESS);
}

int create_tty(int fd, struct termios *prev_pty) {

    struct termios tty_settings;

    if (tcgetattr(STDIN_FILENO, &tty_settings) == -1) {
        perror("Getting TTY attributes failed");
        exit(EXIT_FAILURE); 
    }

    //Save current settings so can restore:
    saved_tty_settings = tty_settings;

    tty_settings.c_lflag &= ~(ICANON | ECHO);
    tty_settings.c_cc[VMIN] = 1;
    tty_settings.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tty_settings) == -1) {
        perror("Setting TTY attributes failed");
        exit(EXIT_FAILURE); 
    }

    return 0;
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

void sigchld_handler(int signal) {
    wait(NULL);

    kill(cpid, SIGTERM);
    exit(1);
}

// Closes the socket and stops the client with the appropriate exit status..
void stop(int socket, int exit_status) {

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty_settings) == -1) {
        perror("Restoring TTY attributes failed");
        exit(EXIT_FAILURE); 
    }

    close_socket(socket);
    exit(exit_status);
}

// Closes the socket and checks for an error.
void close_socket(int socket) {

    // Close the connection.
    if((close(socket)) == -1){
        fprintf(stderr, "Error closing connection, error: %s\n", strerror(errno));
    }
}

// Reads a messagr from a server and returns it as a string (null terminated).
// Also handles read errors internally returning NULL if an error is encountered.
char *read_server_message(int server_fd)
{
  static char msg[MAX_LENGTH];
  int nread;
  
    if ((nread = read(server_fd, msg, MAX_LENGTH - 1)) <= 0) {
        if (errno)
            perror("Error reading from the server socket");
        else
            fprintf(stderr,"Server closed connection unexpectedly\n");
            
        return NULL; 
    }

  msg[nread] = '\0';

  return msg;
}
  
