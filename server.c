#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main() {
    int server_sockfd, client_sockfd;
    int server_len, client_len;

    const char * const CHALLENGE = "<rembash>\n";
    const char * const SECRET = "password";

    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(9734);
    server_len = sizeof(server_address);

    bind(server_sockfd, (struct sockaddr *)&server_address, server_len);

    // Create a connection queue and wait for clients.
    listen(server_sockfd, 5);

    signal(SIGCHLD, SIG_IGN);

    while(1) {
        char ch[4096];

        printf("Server waiting\n");

        // Accept a connection.
        client_len = sizeof(client_address);
        client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_address, &client_len);

        write(client_sockfd,CHALLENGE,strlen(CHALLENGE));
        read(client_sockfd, &ch, 30);
        printf("Server sees = %s", ch);


        if(strcmp(ch, SECRET)) {
            write(client_sockfd, "ok\n", 30);

            if(fork() == 0) {

                // Redirect stdin/stdout/stderr in this process to the client socket.
                // exec /bin/bash (redirections remain in effect)
                // let client know shell is ready by sending <ok>\n
                // when bash subprocess eventually terminates, the client socket is to be closed.

            } else {
                close(client_sockfd);
            }

            exit(0);
        } else {
            write(client_sockfd, "<error>\n", 30);
            close(client_sockfd);
            exit(1);
        }
    }
}
