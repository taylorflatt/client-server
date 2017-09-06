//Includes
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include "readline.c"

//Preprocessor Constants
#define MAX_LENGTH 4096
#define IP "127.0.0.1"
#define PORT 9735
const char * const CHALLENGE = "<rembash>\n";
const char * const PROCEED = "<ok>\n";

int main(int argc, char *argv[]){
    int sock_fd;
    int len;
    struct sockaddr_in serv_address;
    int result;
    int flag = 0;
    char output[MAX_LENGTH];
    char * nextline;
    char * secret;
    char handshake1[MAX_LENGTH];
    char handshake2[MAX_LENGTH];

    char list[MAX_LENGTH];

    char str[MAX_LENGTH];

    // Create client socket.
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error creating socket, error: %s\n", strerror(errno));
        flag = 1;
        exit(1);
    }

    /*  Name the socket, as agreed with the server.  */
    serv_address.sin_family = AF_INET;
    serv_address.sin_addr.s_addr = inet_addr(IP);
    serv_address.sin_port = htons(PORT);

    // Connect the client and server sockets.
    if((connect(sock_fd, (struct sockaddr*)&serv_address, sizeof(serv_address))) == -1) {
        fprintf(stderr, "Error connecting to server, error: %s\n", strerror(errno));
        flag = 1;
        exit(1);
    }

    read(sock_fd, &handshake1, strlen(CHALLENGE));
    //printf("I just read: %s", handshake);

    // Receive challenge and verify expected result.
    if(strcmp(handshake1, CHALLENGE) != 0) {
        fprintf(stderr, "Error receving challenge from server, error: %s\n", strerror(errno));
        flag = 1;
        exit(1);
    }

    // Read SECRET in from stdin.
    secret = readline(STDIN_FILENO);

    // Send our secret to the server for verification.
    if((write(sock_fd, secret, strlen(secret))) == -1) {
        fprintf(stderr, "Error sending secret to server, error: %s\n", strerror(errno));
        flag = 1;
        exit(1);
    }

    read(sock_fd, &handshake2, strlen(PROCEED));

    // Receive the final verification from the server to proceed.
    if(strcmp(handshake2, PROCEED) != 0) {
        printf("Failed secret exchange!");
        printf("\nReadline = %sPROCEED = %s", handshake2, PROCEED);
        exit(1);
    }

    //write(sock_fd,"ls -l; exit\n",12);

    if(fork() == 0) {
        while(fgets(str, 4096, stdin) != NULL) {
            write(sock_fd, str, strlen(str));
        }
    }

    // Read/write.
    while((nextline = readline(sock_fd)) != NULL) {
        if(strlen(nextline) == 1) {
            break;
        } else {
            printf("%s\n", nextline);
        }
    }
    // Close the connection.
    if((close(sock_fd)) == -1){
        fprintf(stderr, "Error closing connection, error: %s\n", strerror(errno));
        flag = -1;
    }

    if(flag == 1)
        return EXIT_FAILURE;
    else
        return EXIT_SUCCESS;
}
  
