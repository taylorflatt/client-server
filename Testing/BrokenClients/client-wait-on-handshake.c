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
#define SECRET "<cs407rembash>\n"
#define CHALLENGE "<rembash>\n"
#define PROCEED "<ok>\n"

//Prototypes
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

struct termios saved_tty_settings;

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

    DTRACE("Client starting: PID=%ld, PGID=%ld, SID=%ld\n",(long)getpid(),(long)getpgrp(),(long)getsid(0));

    // Connect to the server.
    if((server_fd = connect_server(ip)) == -1) {
        perror("Failed to connect to server.");
        exit(EXIT_FAILURE);
    }

    // Set SIGPIPE to ignored so that a write to a closed connection causes an error return
    // rather than a SIGPIPE termination.
    signal(SIGPIPE, SIG_IGN);
    
    // Perform the handshake.
    if(handshake(server_fd) == -1) {
        perror("Failed handshake with the server.");
        exit(EXIT_FAILURE);
    }

    // Set the client's TTY to non-canonical mode.
    if(set_tty_noncanon_noecho() == -1) {
        perror("Failed setting client's terminal settings.");
        exit(EXIT_FAILURE);
    }

    // Make sure our children don't become brain sucking zombies.
    if(create_child_handler_signal() == -1) {
        perror("Error creating signal to handle zombie children.");
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

int connect_server(const char *server_ip) {
    int server_fd;
    struct sockaddr_in serv_address;

    // Create client socket.
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket() failed.");
        return -1;
    }

    // Name the socket, as agreed with the server.
    serv_address.sin_family = AF_INET;
    serv_address.sin_port = htons(PORT);
    inet_aton(server_ip, &serv_address.sin_addr);

    // Connect the client and server sockets.
    if((connect(server_fd, (struct sockaddr*)&serv_address, sizeof(serv_address))) == -1) {
        perror("Connect() failed.");
        return -1;
    }

    return server_fd;
}

int handshake(int server_fd) {
    
    // Receive the challenge.
    char *h_msg;
    if((h_msg = read_handshake_messages(server_fd)) == NULL) {
        perror("Failed reading the challenge from the server.");
        return -1;
    }

    ///
    /// FAILURE POINT
    ///
    sleep(10);

    //  Verify the challenge against known result.
    if(strcmp(h_msg, CHALLENGE) != 0) {
        perror("Incorrect challenge received from the server");
        return -1;
    }

    // Send secret to the server for verification.
    if((write(server_fd, SECRET, strlen(SECRET))) == -1) {
        perror("Failed sending the secret to the server.");
        return -1;
    }

    // Receive server verification.
    if((h_msg = read_handshake_messages(server_fd)) == NULL) {
        perror("Failed to receive the server's PROCEED message.");
        return -1;
    }

    // Receive the final verification from the server to proceed.
    if(strcmp(h_msg, PROCEED) != 0) {
        perror("The server's PROCEED message is invalid.");
        return -1;
    }

    return 0;
}

// Reads a messagr from a server and returns it as a string (null terminated).
// Also handles read errors internally returning NULL if an error is encountered.
char *read_handshake_messages(int client_fd)
{
  static char msg[MAX_LENGTH];
  int nread;
  
    if ((nread = read(client_fd, msg, MAX_LENGTH - 1)) <= 0) {
        if (errno)
            perror("Error reading from the client socket");
        else {
            exit(EXIT_SUCCESS); //TEST INFO HERE_____________________
        }
            
        return NULL; 
    }

  msg[nread] = '\0';

  return msg;
}

int set_tty_noncanon_noecho()
{
    DTRACE("%ld:Setting terminal to non-canon mode.\n",(long)getpid());
    struct termios tty_settings;

    // Get the current terminal settings.
    if (tcgetattr(STDIN_FILENO, &tty_settings) == -1) {
        perror("Getting TTY attributes failed");
        return -1;
    }

    //Save current settings so can restore:
    saved_tty_settings = tty_settings;

    // ECHO won't display the client's text as they type it with putty but ECHONL does.
    tty_settings.c_lflag &= ~(ICANON | ECHO);
    tty_settings.c_lflag |= ISIG;
    tty_settings.c_lflag &= ~ICRNL;
    tty_settings.c_cc[VMIN] = 1;
    tty_settings.c_cc[VTIME] = 0;

    // Put terminal in raw mode after flushing.
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tty_settings) == -1) {
        perror("Setting TTY attributes failed");
        return -1;
    }

    return 0;
}

int create_child_handler_signal() {
    
    DTRACE("%ld:Creating child termination signal handler.\n",(long)getpid());
    struct sigaction act;
    act.sa_handler = &sigchld_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    if(sigaction(SIGCHLD, &act, NULL) == -1) {
        perror("Creation of the signal handler failed!");
        return -1;
    }

    return 0;
}

// Kills off any children and exits.
void sigchld_handler(int sig) {
    
    DTRACE("%ld:Caught signal from subprocess termination...terminating!\n",(long)getpid());
    graceful_exit(EXIT_SUCCESS);
}

void communicate_with_server(int server_fd) {

    pid_t cpid;

    /// CHILD PROCESS
    ///
    /// Reads from STDIN and sends data to the server.
    if((cpid = fork()) == 0) {
        // Since cpid isn't required inside the child, reuse the variable to 
        // store the parent so we can kill it.
        cpid = getppid();
        DTRACE("%ld:Starting data transfer stdin-->socket (FD 0-->%d)\n",(long)getpid(),server_fd);
        if(transfer_data(STDIN_FILENO, server_fd) == -1) {
            perror("Error reading from stdin. ");
            graceful_exit(EXIT_FAILURE);
        }
        DTRACE("%ld:Completed data transfer stdin-->socket\n",(long)getpid());

        graceful_exit(EXIT_SUCCESS);
    }

    /// PARENT PROCESS
    ///
    /// Read from the server and write to the client STDOUT.
    DTRACE("%ld:Starting data transfer socket-->stdout (FD %d-->1)\n",(long)getpid(),server_fd);
    if(transfer_data(server_fd, STDOUT_FILENO) == -1) {
        perror("Error reading from the server and writing to STDOUT.");
    }
    DTRACE("%ld:Completed data transfer socket-->stdout\n",(long)getpid());

    DTRACE("%ld:Normal transfer completion, terminating child (%ld)\n",(long)getpid(),(long)cpid);
    signal(SIGCHLD, SIG_IGN);
    kill(cpid, SIGTERM);

    return;
}

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

// To be called to terminate cleanly.
// Restores TTY settings, collects child,
// and determines success/failure exist status.
void graceful_exit(int exit_status)
{
    DTRACE("%ld:Started exit procedure.\n",(long)getpid());
    restore_tty_settings();

    //Collect child and get its exit status:
    DTRACE("%ld:Cleaning up children.\n",(long)getpid());
    int childstatus;
    wait(&childstatus);

    //Determine if exit status should be failure:
    if (exit_status==EXIT_FAILURE || !WIFEXITED(childstatus) || WEXITSTATUS(childstatus)!=EXIT_SUCCESS) {
        DTRACE("%ld:Error cleaning up children.\n",(long)getpid());
        exit(EXIT_FAILURE);
    }

        DTRACE("%ld:Successfully cleaned up children. Exiting...\n",(long)getpid());

    exit(EXIT_SUCCESS);
}

// Restores initial TTY settings:
void restore_tty_settings()
{
    DTRACE("%ld:Restoring TTY settings.\n",(long)getpid());
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty_settings) == -1) {
        DTRACE("%ld:Failed restoring TTY settings.\n",(long)getpid());
        perror("Restoring TTY attributes failed");
        exit(EXIT_FAILURE);
    }

    return;
}