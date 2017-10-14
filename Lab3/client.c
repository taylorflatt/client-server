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
void sigchld_handler(int signal);
void stop(int socket, int exit_status);

void handshake(int server_fd);
int connect_server(const char *server_ip);

void set_tty_noncanon_noecho();
void restore_tty_settings();
void cleanup_and_exit(int exit_status);

pid_t cpid;
struct termios saved_tty_settings;

int handshake(int server_fd) {

    // Receive the challenge.
    if((handshake = read_server_message(server_fd)) == NULL) {
        perror("Failed reading the challenge from the server.");
        return -1;
    }

    //  Verify the challenge against known result.
    if(strcmp(handshake, CHALLENGE) != 0) {
        perror("Incorrect challenge received from the server");
        return -1;
    }

    // Send secret to the server for verification.
    if((write(server_fd, SECRET, strlen(SECRET))) == -1) {
        perror("Failed sending the secret to the server.");
        return -1;
    }

    // Receive server verification.
    if((handshake = read_server_message(server_fd)) == NULL) {
        perror("Failed to receive the server's PROCEED message.");
        return -1;
    }

    // Receive the final verification from the server to proceed.
    if(strcmp(handshake, PROCEED) != 0) {
        perror("The server's PROCEED message is invalid.");
        return -1;
    }

    return 0;
}

int connect_server(const char *server_ip) {
    int sock_fd;
    struct sockaddr_in servaddr;

    // Create client socket.
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket() failed.")
        return -1;
    }

    // Name the socket, as agreed with the server.
    serv_address.sin_family = AF_INET;
    serv_address.sin_port = htons(PORT);
    inet_aton(ip, &serv_address.sin_addr);

    // Connect the client and server sockets.
    if((connect(sock_fd, (struct sockaddr*)&serv_address, sizeof(serv_address))) == -1) {
        perror("Connect() failed.");
        return -1;
    }

    return sock_fd;
}


int main(int argc, char *argv[]){
    int sock_fd, numBytesRead;
    struct sockaddr_in serv_address;
    char ip[MAX_LENGTH], buf[MAX_LENGTH];
    char *handshake;

    // Check command-line arguments and store ip.
    if(argc != 2) {
        fprintf(stderr, "\nIncorrect number of command line arguments!\n\n");
        printf("    Usage: %s SERVER_IP\n\n", argv[0]);
        exit(EXIT_FAILURE);
    } else {
        strcpy(ip, argv[1]);
    }

    // Connect to the server.
    if((sock_fd = connect_server(ip) == -1) {
        perror("Failed to connect to server.");
        exit(EXIT_FAILURE);
    }
    
    // Perform the handshake.
    if(handshake(sock_fd) == -1) {
        perror("Failed handshake with the server.");
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

    set_tty_noncanon_noecho();

    // Setup the terminal for the client.
    //struct termios prev_pty;
   // if(create_tty(1, &prev_pty) == -1) {
    //    tcsetattr(1, TCSAFLUSH, &prev_pty);
    //    close(sock_fd);
   //     perror("Tcsetattr failed.");
//    }

    // TODO: Swap over to the server method of writing so I'm not using memset.
    // Create a child process.
    if((cpid = fork()) == 0) {
        // Since cpid isn't required inside the child, reuse the variable to 
        // store the parent so we can kill it.
        cpid = getppid();
        while(1) {

            // Read from client's stdin.
            if((numBytesRead = read(STDIN_FILENO, &buf, sizeof(buf))) < 0) {
                if(errno == -1)
                    perror("Child: Failed read from the server!");
                stop(sock_fd, EXIT_FAILURE);
            }

            // The reader exited normally.
            if(numBytesRead == 0) {
                kill(cpid, SIGTERM);
                sigchld_handler(SIGTERM);                
            }

            // Write to server's socket.
            if(write(sock_fd, buf, strlen(buf) + 1) < 0) {
                perror("Child: Failed to write to server.");
                kill(cpid, SIGTERM);
                sigchld_handler(SIGKILL);
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
        if(numBytesRead == 0) {
            sigchld_handler(SIGTERM);
            stop(sock_fd, EXIT_FAILURE);            
        }

        // Write to stdout from the server.
        if(write(STDOUT_FILENO, buf, strlen(buf) + 1) < 0) {
            perror("Parent: Failed to write to server.");
            sigchld_handler(SIGTERM);
            stop(sock_fd, EXIT_FAILURE);
        }

        // Reset the data so no old data is floating around.
        memset(buf, 0, sizeof(buf));
    }

    // Make sure to close the connection upon exiting.
    close_socket(sock_fd);
    //tcsetattr(1, TCSAFLUSH, &prev_pty);
    act.sa_handler = SIG_IGN;

    if(sigaction(SIGCHLD, &act, NULL) == -1)
        perror("Client: Error setting SIGCHLD");

    cleanup_and_exit(EXIT_SUCCESS);
}

// Creates the pty and sets the parameters such as turning off canonical mode.
int create_tty(int fd, struct termios *prev_pty) {
    struct termios pty;

    // Gets the parameters of the pty and stores them in pty.
    if(tcgetattr(fd, &pty) == -1) {
        printf("Could not get the parameters associated with the pty.");
        return -1;        
    }

    if(prev_pty != NULL)
        *prev_pty = pty;

    // Turn canonical mode off and no extended functions.
    pty.c_lflag &= ~(ICANON|IEXTEN);

    // Put terminal in raw mode after flushing.
    if(tcsetattr(fd, TCSAFLUSH, &pty) == -1) {
        printf("Could not set terminal parameters.\n");
        return -1;
    }

    return 0;
}

// Kills off any children and exits.
void sigchld_handler(int sig) {

    kill(cpid, sig);    
    cleanup_and_exit(EXIT_SUCCESS);
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

void set_tty_noncanon_noecho()
{
  struct termios tty_settings;

  if (tcgetattr(STDIN_FILENO, &tty_settings) == -1) {
    perror("Getting TTY attributes failed");
    exit(EXIT_FAILURE); }

  //Save current settings so can restore:
  saved_tty_settings = tty_settings;

  tty_settings.c_lflag &= ~(ICANON | ECHO);
  tty_settings.c_cc[VMIN] = 1;
  tty_settings.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tty_settings) == -1) {
    perror("Setting TTY attributes failed");
    exit(EXIT_FAILURE); }

  return;
}
  

// Restores initial TTY settings:
void restore_tty_settings()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty_settings) == -1) {
    perror("Restoring TTY attributes failed");
    exit(EXIT_FAILURE); }

  return;
}

// To be called to terminate cleanly.
// Restores TTY settings, collects child,
// and determines success/failure exist status.
void cleanup_and_exit(int exit_status)
{
  restore_tty_settings();

  //Collect child and get its exit status:
  int childstatus;
  wait(&childstatus);

  //Determine if exit status should be failure:
  if (exit_status==EXIT_FAILURE || !WIFEXITED(childstatus) || WEXITSTATUS(childstatus)!=EXIT_SUCCESS)
    exit(EXIT_FAILURE);

  exit(EXIT_SUCCESS);
}