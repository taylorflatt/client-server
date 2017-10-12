#define _XOPEN_SOURCE 700
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/tcp.h>
#include <netinet/in.h>

//Preprocessor constants
#define PORT 4070
#define MAX_LENGTH 4096
#define MAX_NUM_CLIENTS 64000
#define SECRET "cs407rembash\n"
#define CHALLENGE "<rembash>\n"
#define PROCEED "<ok>\n"
#define ERROR "<error>\n"

int epoll_fd;

int c_pid[5];

int client_fd_tuples[MAX_NUM_CLIENTS * 2 + 5];
pid_t bash_fd[MAX_NUM_CLIENTS * 2 + 5];

//Prototypes
struct termios tty;
void handle_client(int connect_fd);
void create_processes(int connect_fd);
pid_t pty_open(int *master_fd, int connect_fd, const struct termios *tty);
void sigchld_handler(int signal);
char *read_client_message(int client_fd);
int create_server();

int create_server() {
    int server_sockfd;
    struct sockaddr_in server_address;

    if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Error creating socket, error: %s\n", strerror(errno));
    }

    int i=1;
    if(setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i))) {
        fprintf(stderr, "Error setting sockopt.");
        return -1;
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

    return server_sockfd;
}

int main(int argc, char *argv[]) {
    int connect_fd, server_sockfd;
    socklen_t client_len;
    
    struct sockaddr_in client_address;

    if((server_sockfd = create_server()) == -1) {
        exit(EXIT_FAILURE);
    }

    if(signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        fprintf(stderr, "Error setting SIGCHLD to SIG_IGN.");
        exit(EXIT_FAILURE);
    }

    // Main server loop
    while(1) {
        // Collect any terminated children before attempting to accept a new connection.
        while (waitpid(-1,NULL,WNOHANG) > 0);
        
        // Accept a new connection and get socket to use for client:
        client_len = sizeof(client_address);
        if((connect_fd = accept(server_sockfd, (struct sockaddr *) &client_address, &client_len)) == -1) {
            fprintf(stderr, "Error making connection, error: %s\n", strerror(errno));
        }
        
        // Fork immediately.
        if(connect_fd != -1) {
            if(fork() == 0) {
                close(server_sockfd);
                handle_client(connect_fd);
            }
            close(connect_fd);
        }
    }

    exit(EXIT_SUCCESS);
}

// Handles the three-way handshake and starts up the create_processes method.
void handle_client(int connect_fd) {
    
    char *pass;

    // Send challenge to client.
    printf("Sending challenge to client.\n");

    write(connect_fd, CHALLENGE, strlen(CHALLENGE));

    // TIMER FOR SIGNAL HANDLER
    // Read password from client.
    if((pass = read_client_message(connect_fd)) == NULL)
        return;
    // STOP TIMER.
    
    // Make sure the password is good.
    if(strcmp(pass, SECRET) == 0) {
        printf("Challenge passed. Moving into accept client.\n");

        // let client know shell is ready by sending <ok>\n
        write(connect_fd, PROCEED, strlen(PROCEED));
        create_processes(connect_fd);
        
    }  else {
        // Invalid secret, tell the client.
        write(connect_fd, ERROR, strlen(ERROR));
        fprintf(stderr, "Aborting connection. Invalid secret: %s", pass);
    }
}

// Creates two processes which read and write on a socket connecting the client and server.
void create_processes(int connect_fd) {
    
    int master_fd;

    struct sigaction act;
    act.sa_handler = sigchld_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);

    if(sigaction(SIGCHLD, &act, NULL) == -1) {
        perror("Creation of the signal handler failed!");
        exit(1);
    }

    printf("Setting c_pid[0] to pty_open.\n");

    if((c_pid[0] = pty_open(&master_fd, connect_fd, &tty)) == -1) 
        exit(1);
    
    // Reads from the client (socket) and writes to the master.
    char buf[MAX_LENGTH];
    pid_t pid;
    if((pid = fork()) == 0) {
        while(1) {
            if(read(connect_fd, &buf, 1) != 1)
                break;
            if(write(master_fd, &buf, 1) != 1)
                break;

            printf("Child wrote %c from sock to master\n", buf[0]);
        }

        exit(0);
    }

    c_pid[1] = pid;
    printf("PID of child socketreader: %d\n", (int)pid);

    // Reads from the master and writes output back to client (socket).
    while(1) {
        int nwrite, total, read_len;
        nwrite = 0;
        
        while(nwrite != -1 && (read_len = read(master_fd, &buf, MAX_LENGTH))) {
            printf("read_len to master:%d, lenwrote to socket:%d\n", read_len, nwrite);
            total = 0;

            // Be careful that the appropriate writes are sent. Buf is not wiped.
            do {
                if((nwrite = write(connect_fd, total + buf, read_len - total)) == -1)
                    break;
            } while((total += nwrite) < read_len);
        }
    }

    close(connect_fd);
    close(master_fd);

    act.sa_handler = SIG_IGN;

    if(sigaction(SIGCHLD, &act, NULL) == -1)
        perror("Client: Error setting SIGCHLD");
    return;
}

// Creates the master and slave pty.
pid_t pty_open(int *master_fd, int connect_fd, const struct termios *tty) {
    
    char * slavename;
    int massa, slave_fd, err;

    /* Open an unused pty dev and store the fd for later reference.
        O_RDWR := Open pty for reading + writing. 
        O_NOCTTY := Don't make it a controlling terminal for the process. 
    */
    if((massa = posix_openpt(O_RDWR|O_NOCTTY)) == -1)
        return -1;
    
    /* Attempt to kickstart the master.
        Grantpt := Creates a child process that executes a set-user-ID-root program changing ownership of the slave 
            to be the same as the effective user ID of the calling process and changes permissions so the owner 
            has R/W permissions.
        Unlockpt := Removes the internal lock placed on the slave corresponding to the pty. (This must be after grantpt).
        ptsname := Returns the name of the pty slave corresponding to the pty master referred to by massa. (/dev/pts/nn).
    */
    printf("Before granting pt\n");
    if(grantpt(massa) == -1 || unlockpt(massa) == -1 || (slavename = ptsname(massa)) == NULL) {
        err = errno;
        close(massa);
        errno = err;
        return -1;
    }
    
    // Child
    pid_t c_pid;
    if((c_pid = fork()) == 0) {
        printf("slavename = %s\n", slavename);
        
        // Massa is not needed in the child, close it.
        close(massa);
        close(connect_fd);

        if(setsid() == -1) {
            printf("Could not create a new session.\n");
            return -1;            
        }

        if((slave_fd = open(slavename, O_RDWR)) == -1) {
            printf("Could not open %s\n", slavename);
            return -1;
        }
        
        if(tcsetattr(slave_fd, TCSANOW, tty) == -1) {
            printf("Could not set the set the terminal parameters.\n");
            return -1;
        }

        printf("Setting dup\n");
        if(dup2(slave_fd, STDIN_FILENO) < 0) {
            fprintf(stderr, "Stdin redirection error.\n");
            return -1;
        }
        if(dup2(slave_fd, STDOUT_FILENO) < 0) {
            fprintf(stderr, "Stdout redirection error.\n");
            return -1;
        }
        if(dup2(slave_fd, STDERR_FILENO) < 0) {
            fprintf(stderr, "Stderr redirection error.\n");
            return -1;
        }

        execlp("bash", "bash", NULL);

        if(slave_fd > STDERR_FILENO)
            close(slave_fd);

        // Shouldn't ever go this far.
        //return -1;
    }

    printf("slave pid = %d\n", (int)c_pid);

    * master_fd = massa;
    return c_pid;
}

// Collects processes.
void sigchld_handler(int sig) {
    wait(NULL);

    char read_string[MAX_LENGTH];

    char *filename = "trump.txt";
    FILE *fptr = NULL;
 
    if((fptr = fopen(filename,"r")) == NULL)
        fprintf(stderr,"error opening %s\n",filename);
 
    while(fgets(read_string,sizeof(read_string),fptr) != NULL) {
        printf("%s",read_string);
    }

    int r;
    switch(r = rand() % 6 + 1) {
        case 1:
            printf("In the old days, children like these would be carried out on stretchers. Processes %d and %d have been cleaned up!\n", c_pid[0], c_pid[1]);
            break;
        case 2:
            printf("I'd like to punch him in the face! Processes %d and %d have been cleaned up!\n", c_pid[0], c_pid[1]);
            break;
        case 3:
            printf("The concept of Zombies was created by and for the Chinese in order to make U.S. manufacturing non-competitive. Processes %d and %d have been cleaned up!\n", c_pid[0], c_pid[1]);
            break;
        case 4:
            printf("Do you think hands like these would forget to reap these children? Processes %d and %d have been cleaned up!\n", c_pid[0], c_pid[1]);    
            break;
        case 5:
            printf("My ability to handle children and signals is the best. The greatest. Wonderful. No one questions it. Look at me. Everyone knows I know what I'm doing. Processes %d and %d have been cleaned up!\n", c_pid[0], c_pid[1]);
            break;
        case 6:
            printf("Why can't we use nuclear weapons? Processes must be destroyed when no longer wanted! Processes %d and %d have been cleaned up!\n", c_pid[0], c_pid[1]);
            break;
    }

    fclose(fptr);

    kill(c_pid[0], sig);
    kill(c_pid[1], sig);
    exit(0);
}

// Reads a messagr from a server and returns it as a string (null terminated).
// Also handles read errors internally returning NULL if an error is encountered.
char *read_client_message(int client_fd)
{
  static char msg[MAX_LENGTH];
  int nread;
  
    if ((nread = read(client_fd, msg, MAX_LENGTH - 1)) <= 0) {
        if (errno)
            perror("Error reading from the client socket");
        else
            fprintf(stderr, "Client closed connection unexpectedly\n");
            
        return NULL; 
    }

  msg[nread] = '\0';

  return msg;
}