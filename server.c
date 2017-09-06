//Includes
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
#define PORT 9735
const char * const CHALLENGE = "<rembash>\n";
const char * const SECRET = "password";
const char * const PROCEED = "<ok>\n";

//Prototypes
void handle_client(int connect_fd, int flag);

int main(int argc, char *argv[]){
    int listen_fd, connect_fd, flag = 0;
    int server_sockfd, client_sockfd;
    int client_len;
    
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        fprintf(stderr, "Error creating socket, error: %s\n", strerror(errno));
        flag = 1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT);

    if((bind(server_sockfd, (struct sockaddr *) &server_address, sizeof(server_address))) == -1){
        fprintf(stderr, "Error assigning address to socket, error: %s\n", strerror(errno));
        flag = 1;
    }

    // Start listening to server socket.
    if((listen(server_sockfd, 10)) == -1){
        fprintf(stderr, "Error listening to socket, error: %s\n", strerror(errno));
        flag = 1;
    }

    int i=1;
    setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

  //Main server loop:
    while(1){
        //Accept a new connection and get socket to use for client:
        client_len = sizeof(client_address);
        if((connect_fd = accept(server_sockfd, (struct sockaddr *) &client_address, &client_len)) == -1){
            fprintf(stderr, "Error making connection, error: %s\n", strerror(errno));
            flag = 1;
        }
        //Read client messages.
        handle_client(connect_fd, flag);

        //if((close(connect_fd)) == -1){
        //    fprintf(stderr, "Error closing connection, error: %s\n", strerror(errno));
        //    flag = 1;
        //}
    }
    if(flag == 1)
        return EXIT_FAILURE;
    else
        return EXIT_SUCCESS;
}

void handle_client(int connect_fd, int flag){
    char msg[201], msgfix[195];
    int nread;

    // Send challenge to client.
    printf("Sending challenge to client.");
    write(connect_fd, CHALLENGE, strlen(CHALLENGE));
    
    //nread = read(connect_fd, msg, 200);
    //msg[nread] = '\0';
    nread = read(connect_fd, &msg, 8);

    if(strcmp(msg, SECRET) == 0) {
        if(fork() == 0) {

            // Redirect stdin/stdout/stderr in this process to the client socket.
            if(dup2(connect_fd, 0) < 0){
                fprintf(stderr, "Redirection error.\n");
                exit(EXIT_FAILURE);
            }
            if(dup2(connect_fd, 1) < 0){
                fprintf(stderr, "Redirection error.\n");
                exit(EXIT_FAILURE);
            }
            if(dup2(connect_fd, 2) < 0){
                fprintf(stderr, "Redirection error.\n");
                exit(EXIT_FAILURE);
            }

            write(connect_fd, PROCEED, strlen(PROCEED));

            // exec /bin/bash (redirections remain in effect)
            execlp("bash","bash","--noediting","-i",NULL);

            // let client know shell is ready by sending <ok>\n
            

            // when bash subprocess eventually terminates, the client socket is to be closed.
            if((close(connect_fd)) == -1){
                fprintf(stderr, "Error closing connection, error: %s\n", strerror(errno));
            }
        } else {
            close(connect_fd);
        }
    } else {
        printf("Aborting connection. Invalid secret: %s", msg);
    }

//    exit(0);
}
