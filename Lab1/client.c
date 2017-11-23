#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

// Preprocessor Constants
#define MAX_LENGTH 4096
#define PORT 4070
#define SECRET "cs407rembash\n"
#define CHALLENGE "<rembash>\n"
#define PROCEED "<ok>\n"

//Prototypes
void close_socket(int connect_fd);
char *read_server_message(int server_fd);
void stop(int socket, int exit_status);

int main(int argc, char *argv[]){
    int sock_fd, numBytesRead, status;
    struct sockaddr_in serv_address;
    char ip[MAX_LENGTH], buf[MAX_LENGTH];
    char *handshake;
    pid_t cpid, pid;

    // Check command-line arguments and store ip.
    if(argc != 2) {
        fprintf(stderr, "\nIncorrect number of command line arguments!\n\n");
        printf("    Usage: %s SERVER_IP\n\n", argv[0]);
        exit(EXIT_FAILURE);
    } else {
        strcpy(ip, argv[1]);
    }

    // Create client socket.
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error creating socket, error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Name the socket, as agreed with the server.
    serv_address.sin_family = AF_INET;
    serv_address.sin_port = htons(PORT);
    inet_aton(ip, &serv_address.sin_addr);

    // Connect the client and server sockets.
    if((connect(sock_fd, (struct sockaddr*)&serv_address, sizeof(serv_address))) == -1) {
        fprintf(stderr, "Error connecting to server, error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Receive the challenge.
    if((handshake = read_server_message(sock_fd)) == NULL) {
        exit(EXIT_FAILURE);
    }

    //  Verify the challenge against known result.
    if(strcmp(handshake, CHALLENGE) != 0) {
        fprintf(stderr, "Error receving challenge from server, error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Send secret to the server for verification.
    if((write(sock_fd, SECRET, strlen(SECRET))) == -1) {
        fprintf(stderr, "Error sending secret to server, error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Receive server verification.
    if((handshake = read_server_message(sock_fd)) == NULL) {
        exit(EXIT_FAILURE);
    }

    // Receive the final verification from the server to proceed.
    if(strcmp(handshake, PROCEED) != 0) {
        printf("%s", handshake);
        exit(EXIT_FAILURE);    
    }

    // Create a child process.
    if((cpid = fork()) == 0) {
        while(1) {

            // Read from client's stdin.
            if((numBytesRead = read(STDIN_FILENO, &buf, sizeof(buf))) < 0) {
                if(errno == -1)
                    perror("Child: Failed read from the server!");
                stop(sock_fd, EXIT_FAILURE);
            }

            // The reader exited normally.
            if(numBytesRead == 0)
                stop(sock_fd, EXIT_FAILURE);

            // Write to server's socket.
            if(write(sock_fd, buf, strlen(buf) + 1) < 0) {
                perror("Child: Failed to write to server.");
                stop(sock_fd, EXIT_FAILURE);
            }
            
            // Reset the data so no old data is floating around.
            memset(buf, 0, sizeof(buf));
        }
    }

    // Parent
    while(1) {

        // Read from the server socket.
        if((numBytesRead = read(sock_fd, &buf, sizeof(buf))) < 0) {
            perror("Parent: Failed to read from the server.");
            stop(sock_fd, EXIT_FAILURE);
        }

        // The reader exited normally.
        if(numBytesRead == 0)
            stop(sock_fd, EXIT_FAILURE);

        // Write to stdout from the server.
        if(write(STDOUT_FILENO, buf, strlen(buf) + 1) < 0) {
            perror("Parent: Failed to write to server.");
            stop(sock_fd, EXIT_FAILURE);
        }

        // Reset the data so no old data is floating around.
        memset(buf, 0, sizeof(buf));

        // Check if the child is still running. If not, clean it up. (WNOHANG = nonblocking)
        if((pid = waitpid(cpid, &status, WNOHANG) != 0))
            stop(sock_fd, cpid);
    }

    // Make sure to close the connection upon exiting.
    stop(sock_fd, EXIT_SUCCESS);
    close_socket(sock_fd);

    exit(EXIT_SUCCESS);
}

// Closes the socket and stops the client with the appropriate exit status..
void stop(int socket, int exit_status) {

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
  
