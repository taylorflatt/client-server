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
#include "readline.c"

// Preprocessor Constants
#define MAX_LENGTH 4096
#define PORT 9735
#define SECRET "password\n"
#define CHALLENGE "<rembash>\n"
#define PROCEED "<ok>\n"

//Prototypes
void close_socket(int connect_fd);

int main(int argc, char *argv[]){
    int sock_fd, numBytesRead, status;
    struct sockaddr_in serv_address;
    char ip[MAX_LENGTH], handshake[MAX_LENGTH], buf[MAX_LENGTH];
    pid_t cpid, pid;

    // Check command-line arguments and store ip.
    if(argc != 2) {
        fprintf(stderr, "\nIncorrect number of command line arguments!\n\n");
        printf("    Usage: %s SERVER_IP\n\n", argv[0]);
        exit(1);
    } else {
        strcpy(ip, argv[1]);
    }

    // Create client socket.
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error creating socket, error: %s\n", strerror(errno));
        exit(1);
    }

    // Name the socket, as agreed with the server.
    serv_address.sin_family = AF_INET;
    serv_address.sin_addr.s_addr = inet_addr(ip);
    serv_address.sin_port = htons(PORT);

    // Connect the client and server sockets.
    if((connect(sock_fd, (struct sockaddr*)&serv_address, sizeof(serv_address))) == -1) {
        fprintf(stderr, "Error connecting to server, error: %s\n", strerror(errno));
        exit(1);
    }
    
    // Receive the challenge.
    read(sock_fd, &handshake, strlen(CHALLENGE));

    //  Verify the challenge against known result.
    if(strcmp(handshake, CHALLENGE) != 0) {
        fprintf(stderr, "Error receving challenge from server, error: %s\n", strerror(errno));
        exit(1);
    }

    // Send secret to the server for verification.
    if((write(sock_fd, SECRET, strlen(SECRET))) == -1) {
        fprintf(stderr, "Error sending secret to server, error: %s\n", strerror(errno));
        exit(1);
    }

    // Receive server verification.
    memset(handshake, 0, sizeof(handshake));
    read(sock_fd, &handshake, sizeof(handshake));

    // Receive the final verification from the server to proceed.
    if(strncmp(handshake, PROCEED, strlen(PROCEED)) != 0) {
        printf("%s", handshake);
        exit(1);    
    }

    // Create a child process.
    if((cpid = fork()) == 0) {
        while(1) {

            // Read from client's stdin.
            if((numBytesRead = read(STDIN_FILENO, &buf, sizeof(buf))) < 0) {
                perror("Failed read from the server CHILD!");
                close_socket(sock_fd);
                exit(1);
            }

            // The reader exited normally.
            if(numBytesRead == 0) {
                close_socket(sock_fd);
                exit(1);
            }

            // Write to server's socket.
            if(write(sock_fd, buf, strlen(buf) + 1) < 0) {
                perror("Failed to write to server.");
                close_socket(sock_fd);
                exit(1);
            }
            
            // Reset the data so no old data is floating around.
            memset(buf, 0, sizeof(buf));
        }
    }

    // Parent
    while(1) {

        // Read from the server socket.
        if((numBytesRead = read(sock_fd, &buf, sizeof(buf))) < 0) {
            perror("Failed to read from the server.");
            close_socket(sock_fd);
            exit(1);
        }

        // The reader exited normally.
        if(numBytesRead == 0) {
            close_socket(sock_fd);
            exit(1);
        }

        // Write to stdout from the server.
        if(write(STDOUT_FILENO, buf, strlen(buf) + 1) < 0) {
            perror("Failed to write to server.");
            close_socket(sock_fd);
            exit(1);
        }

        // Reset the data so no old data is floating around.
        memset(buf, 0, sizeof(buf));

        // Check if the child is still running. If not, clean it up.
        if((pid = waitpid(cpid, &status, WNOHANG) != 0)) {
            close_socket(sock_fd);
            exit(cpid);
        }
    }

    close_socket(sock_fd);

    exit(0);
}

void close_socket(int socket) {

    // Close the connection.
    if((close(socket)) == -1){
        fprintf(stderr, "Error closing connection, error: %s\n", strerror(errno));
    }
}
  
