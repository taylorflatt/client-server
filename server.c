#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <stdlib.h>
#include <errno.h>

//Preprocessor constants
#define PORT 4070
#define MAX_LENGTH 4096
#define SECRET "cs407rembash\n"
#define CHALLENGE "<rembash>\n"
#define PROCEED "<ok>\n"
#define ERROR "<error>\n"

//Prototypes
void handle_client(int connect_fd);

int main(int argc, char *argv[]) {
    int connect_fd, server_sockfd;
    socklen_t client_len;
    
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        fprintf(stderr, "Error creating socket, error: %s\n", strerror(errno));
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

    int i=1;
    setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

  //Main server loop:
    while(1){
        //Accept a new connection and get socket to use for client:
        client_len = sizeof(client_address);
        if((connect_fd = accept(server_sockfd, (struct sockaddr *) &client_address, &client_len)) == -1) {
            fprintf(stderr, "Error making connection, error: %s\n", strerror(errno));
        }

        handle_client(connect_fd);

        //if((close(connect_fd)) == -1){
        //    fprintf(stderr, "Error closing connection, error: %s\n", strerror(errno));
        //}
    }

    exit(0);
}

void handle_client(int connect_fd) {

    char pass[MAX_LENGTH];

    // Send challenge to client.
    printf("Sending challenge to client.");
    write(connect_fd, CHALLENGE, strlen(CHALLENGE));

    // Read password from client.
    read(connect_fd, &pass, sizeof(pass));

    // Make sure the password is good.
    if(strcmp(pass, SECRET) == 0) {

        // let client know shell is ready by sending <ok>\n
        write(connect_fd, PROCEED, strlen(PROCEED));

        if(fork() == 0) {

        // (Required) Handle multiple clients.
        if(setsid() < 0) {
            fprintf(stderr, "Redirection error.\n");
            exit(EXIT_FAILURE);
        }

        // Redirect stdin/stdout/stderr in this process to the client socket.
        if(dup2(connect_fd, 0) < 0) {
            fprintf(stderr, "Redirection error.\n");
            exit(EXIT_FAILURE);
        }
        if(dup2(connect_fd, 1) < 0) {
            fprintf(stderr, "Redirection error.\n");
            exit(EXIT_FAILURE);
        }
        if(dup2(connect_fd, 2) < 0) {
            fprintf(stderr, "Redirection error.\n");
            exit(EXIT_FAILURE);
        }

        // Exec /bin/bash (redirections remain in effect)
        execlp("bash", "bash", "--noediting", "-i", NULL);

        // When bash subprocess eventually terminates, the client socket is to be closed.
        if((close(connect_fd)) == -1) {
            fprintf(stderr, "Error closing connection, error: %s\n", strerror(errno));
        }

        } else {
            close(connect_fd);
        }
        
    }  else {
        // Invalid secret, tell the client.
        write(connect_fd, ERROR, strlen(ERROR));
        fprintf(stderr, "Aborting connection. Invalid secret: %s", pass);
    }
}
