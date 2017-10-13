/// Command-line compile arguments
///
/// Pthreads: -pthread
/// Timers: -lrt
/// Sample: gcc -Wall -pthread -lrt -o server server.c


#define _POSIX_C_SOURCE 200809L // Required for timers.
#define _XOPEN_SOURCE 700 // Required for pty.
#define _GNU_SOURCE // Required for syscall.

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

//Preprocessor constants
#define PORT 4070
#define MAX_LENGTH 4096
#define MAX_NUM_CLIENTS 64000
#define MAX_EVENTS 24
#define SECRET "cs407rembash\n"
#define CHALLENGE "<rembash>\n"
#define PROCEED "<ok>\n"
#define ERROR "<error>\n"

int epoll_fd;

// A map to store the fd for pty/socket.
int client_fd_tuples[MAX_NUM_CLIENTS * 2 + 5];
pid_t bash_fd[MAX_NUM_CLIENTS * 2 + 5];

//Prototypes
struct termios tty;
void *handle_client(void *client_fd_ptr);
int pty_open(int client_fd, const struct termios *tty);
void sigchld_handler(int signal);
char *read_client_message(int client_fd);

int create_server();
void *epoll_listener(void * ignore);
void sighandshake_handler(int signal, siginfo_t * sip, void * ignore);
int transfer_data(int from, int to);
int create_bash_process(char *pty_slave, const struct termios *tty);

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

void *epoll_listener(void * ignore) {

    struct epoll_event ev_list[MAX_EVENTS];
    int events;
    int i;

    while(1) {
        events = epoll_wait(epoll_fd, ev_list, MAX_EVENTS, -1);

        if(events == -1) {
            if(errno == EINTR) {
                continue;
            } else {
                fprintf(stderr, "Epoll loop error.\n");
                exit(EXIT_FAILURE);
            }
        }

        printf("Epoll sees %d events \n", events);

        for(i = 0; i < events; i++) {
            if(ev_list[i].events & EPOLLIN) {
                if(transfer_data(ev_list[i].data.fd, client_fd_tuples[ev_list[i].data.fd])) {
                    fprintf(stderr, "Error reading/writing to the client. Closing shop.\n");
                    kill(bash_fd[ev_list[i].data.fd], SIGTERM);
                    //close(ev_list[i].data.fd);
                    close(client_fd_tuples[ev_list[i].data.fd]);
                } else if(ev_list[i].events & (EPOLLHUP | EPOLLERR)) {
                    close(client_fd_tuples[ev_list[i].data.fd]);
                }
            }
        }
    }
}

int transfer_data(int from, int to) {

    char buf[MAX_LENGTH];
    static ssize_t nread;

    while((nread = read(from, buf, MAX_LENGTH)) > 0) {
        if(write(to, buf, nread) == -1) {
            fprintf(stderr, "Failed writing data.");
            break;
        }
    }

    if(nread == -1 && errno != EWOULDBLOCK && errno != EAGAIN) {
        fprintf(stderr, "Failed reading data.");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int client_fd, server_sockfd;
    int *client_fd_ptr;
    pthread_t thread_id;
    socklen_t client_len;
    
    struct sockaddr_in client_address;

    if((server_sockfd = create_server()) == -1) {
        exit(EXIT_FAILURE);
    }

    if(signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        fprintf(stderr, "Error setting SIGCHLD to SIG_IGN.\n");
        exit(EXIT_FAILURE);
    }

    if((epoll_fd = epoll_create1(EPOLL_CLOEXEC)) == -1) {
        fprintf(stderr, "Error creating EPOLL.\n");
        exit(EXIT_FAILURE);
    }

    pthread_create(&thread_id, NULL, &epoll_listener, NULL);

    /// Client accept loop.
    ///
    /// Server will create a pthread which sits and listens for new clients and then
    /// runs the initial handshake between client/server.
    while(1) {
        // Collect any terminated children before attempting to accept a new connection.
        while (waitpid(-1,NULL,WNOHANG) > 0);
        
        // Accept a new connection and get socket to use for client:
        client_len = sizeof(client_address);
        if((client_fd = accept(server_sockfd, (struct sockaddr *) &client_address, &client_len)) == -1) {
            fprintf(stderr, "Error making connection, error: %s\n", strerror(errno));
        }


        client_fd_ptr = (int *) malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        if(pthread_create(&thread_id, NULL, &handle_client, client_fd_ptr)) {
            fprintf(stderr, "Error creating the accept temporary pthread.\n");
        }
    }

    exit(EXIT_SUCCESS);
}

int handshake(int client_fd) {

    // Three second timer.
    static struct itimerspec timer;
    timer.it_value.tv_sec = 5;

    // Create the signal action.
    struct sigaction sig_act;
    sig_act.sa_flags = SA_SIGINFO;
    sig_act.sa_sigaction = &sighandshake_handler;

    // Create a signal event tied to a specific thread.
    struct sigevent sig_ev;
    sig_ev.sigev_signo = SIGALRM;
    sig_ev.sigev_notify = SIGEV_THREAD_ID;

    int alarm_flag = 0;
    char *pass;
    timer_t timer_id;


    sigemptyset(&sig_act.sa_mask);

    if(sigaction(SIGALRM, &sig_act, NULL) == -1) {
        fprintf(stderr, "Error setting up sigaction.\n");
    }

    // Setup the signal event with the appropriate flags and assign to a 
    // specific thread.
    sig_ev.sigev_value.sival_ptr = &alarm_flag;
    sig_ev._sigev_un._tid = syscall(SYS_gettid);

    if(timer_create(CLOCK_REALTIME, &sig_ev, &timer_id) == -1) {
        fprintf(stderr, "Error creating handshake timer.\n");
    }

    if(timer_settime(timer_id, 0, &timer, NULL) == -1) {
        fprintf(stderr, "Error setting handshake timer duration.\n");
    }

    // Sending the challenge to the client.
    if((alarm_flag || write(client_fd, CHALLENGE, strlen(CHALLENGE))) == -1) {
        fprintf(stderr, "Server took too long sending message or write failed. AlarmFlag: " + alarm_flag);
        return -1;
    }

    if(alarm_flag || (pass = read_client_message(client_fd)) == NULL) {
        fprintf(stderr, "Client took too long sending message or read failed. AlarmFlag: " + alarm_flag);
        return -1;
    }

    if(alarm_flag || strcmp(pass, SECRET) != 0) {
        fprintf(stderr, "Server took too long comparing the challenge, the compare failed, or invalid secret. AlarmFlag: " + alarm_flag);
        write(client_fd, ERROR, strlen(ERROR));
        return -1;
    } else {
        write(client_fd, PROCEED, strlen(PROCEED));
    }

    if(signal(SIGALRM, SIG_IGN) == SIG_ERR) {
        fprintf(stderr, "Failed to ignore the handshake signal.");
    }

    if(timer_delete(timer_id) == -1) {
        fprintf(stderr, "Failed to delete the handshake timer.");
    }

    return 0;
}

// Empty signal handler for the handshake.
void sighandshake_handler(int signal, siginfo_t * sip, void * ignore)
{
    fprintf(stdout, "Alarm has a value: %d\n", *(int *) (sip->si_ptr));
    *(int *) sip->si_ptr = 1;
}

// Handles the three-way handshake and starts up the create_processes method.
void  *handle_client(void *client_fd_ptr) {

    int flags;

    // Dereference the int pointer and get rid of the memory now that we no longer need it.
    int client_fd = *(int *) client_fd_ptr;
    free(client_fd_ptr);

    // Conduct the three-way handshake with the client.
    if(handshake(client_fd) == -1) {
        fprintf(stderr, "Client failed the handshake.\n");
        close(client_fd);
        return NULL;
    }

    /// Need to get the current fd's flags and then add non-blocking and set them.
    if((flags = fcntl(client_fd, F_GETFL, 0)) == -1) {
        fprintf(stderr, "Failed to get flags.\n");
    }

    flags |= O_NONBLOCK;

    if(fcntl(client_fd, F_SETFL, flags) == -1) {
        fprintf(stderr, "Failed to set non-blocking flags.\n");
    }

    if(pty_open(client_fd, &tty) == -1) {
        fprintf(stderr, "Failed to open pty and start bash.\n");
    }

    return NULL;
}

// Creates the master and slave pty.
int pty_open(int client_fd, const struct termios *tty) {
    
    char * pty_slave;
    int pty_master, err, flags;
    struct epoll_event ep_ev[2];
    pid_t b_pid;

    /* Open an unused pty dev and store the fd for later reference.
        O_RDWR := Open pty for reading + writing. 
        O_NOCTTY := Don't make it a controlling terminal for the process. 
    */
    if((pty_master = posix_openpt(O_RDWR|O_NOCTTY|O_CLOEXEC)) == -1) {
        fprintf(stderr, "Failed openpt.\n");
        return -1;        
    }

    /// Need to get the pty_master fd's flags and then add non-blocking and set them.
    if((flags = fcntl(pty_master, F_GETFL, 0)) == -1) {
        fprintf(stderr, "Failed to get flags.\n");
    }

    flags |= O_NONBLOCK;

    if(fcntl(pty_master, F_SETFL, flags) == -1) {
        fprintf(stderr, "Failed to set non-blocking flags.\n");
    }
    
    /* Attempt to kickstart the master.
        Grantpt := Creates a child process that executes a set-user-ID-root program changing ownership of the slave 
            to be the same as the effective user ID of the calling process and changes permissions so the owner 
            has R/W permissions.
        Unlockpt := Removes the internal lock placed on the slave corresponding to the pty. (This must be after grantpt).
        ptsname := Returns the name of the pty slave corresponding to the pty master referred to by pty_master. (/dev/pts/nn).
    */
    printf("Before granting pt\n");
    if(grantpt(pty_master) == -1 || unlockpt(pty_master) == -1 || (pty_slave = ptsname(pty_master)) == NULL) {
        err = errno;
        close(pty_master);
        errno = err;
        return -1;
    }

    pty_slave = (char *) malloc(1024);
    strcpy(pty_slave, ptsname(pty_master));

    /// Create the bash process.
    ///
    /// Fork off a new bash process and redirect stdin/stdout/stderr appropriately.
    if((b_pid = fork()) == 0) {
        printf("pty_slave = %s\n", pty_slave);
        
        // pty_master is not needed in the child, close it.
        close(pty_master);
        close(client_fd);
        if(create_bash_process(pty_slave, tty) == -1) {
            fprintf(stderr, "Failed to create bash process.\n");
        }
    }

    printf("slave pid = %d\n", (int)b_pid);
    free(pty_slave);

    client_fd_tuples[client_fd] = pty_master;
    client_fd_tuples[pty_master] = client_fd;

    bash_fd[client_fd] = b_pid;
    bash_fd[pty_master] = b_pid;

    // Setup the epoll event handling (R/W).
    ep_ev[0].data.fd = client_fd;
    ep_ev[0].events = EPOLLIN | EPOLLET;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, ep_ev);

    ep_ev[1].data.fd = pty_master;
    ep_ev[1].events = EPOLLIN | EPOLLET;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pty_master, ep_ev + 1);
    
    return 0;
}

int create_bash_process(char *pty_slave, const struct termios *tty) {

    int slave_fd;

    if(setsid() == -1) {
        printf("Could not create a new session.\n");
        return -1;            
    }

    if((slave_fd = open(pty_slave, O_RDWR)) == -1) {
        printf("Could not open %s\n", pty_slave);
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

    free(pty_slave);
    execlp("bash", "bash", NULL);

    fprintf(stderr, "Failed to exec bash!\n");

    if(slave_fd > STDERR_FILENO)
        close(slave_fd);

    return -1;
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