/// Command-line compile arguments
///
/// Pthreads: -pthread
/// Timers: -lrt
/// Sample: gcc -Wall -pthread -lrt -DDEBUG -o server server.c

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
#include "DTRACE.h"

//Preprocessor constants
#define PORT 4070
#define MAX_LENGTH 4096
#define MAX_NUM_CLIENTS 64000
#define MAX_EVENTS 24
#define SECRET "cs407rembash\n"
#define CHALLENGE "<rembash>\n"
#define PROCEED "<ok>\n"
#define ERROR "<error>\n"

// Epoll file descriptor.
int epoll_fd;

// A map to store the fd for pty/socket.
int client_fd_tuples[MAX_NUM_CLIENTS * 2 + 5];
pid_t bash_fd[MAX_NUM_CLIENTS * 2 + 5];
struct termios tty;

//Prototypes
int create_server();
void *epoll_listener(void * ignore);
void *handle_client(void *client_fd_ptr);
int handshake(int client_fd);
int pty_open(int client_fd, const struct termios *tty);
int create_bash_process(char *pty_slave, const struct termios *tty);
int set_nonblocking_fd(int fd);
void sighandshake_handler(int signal, siginfo_t * sip, void * ignore);
char *read_client_message(int client_fd);
int transfer_data(int from, int to);

int main(int argc, char *argv[]) {
    int client_fd, server_sockfd;
    int *client_fd_ptr;
    pthread_t thread_id;
    socklen_t client_len;
    
    struct sockaddr_in client_address;

    if((server_sockfd = create_server()) == -1) {
        perror("Error creating the server.");
        exit(EXIT_FAILURE);
    }

    // Force writes to closed sockets to return an error rather than a signal.
    if(signal(SIGPIPE,SIG_IGN) == SIG_ERR) {
        perror("Error setting SIGPIPE to SIG_IGN.");
        exit(EXIT_FAILURE);
    }

    if(signal(SIGCHLD, SIG_IGN) == SIG_ERR) {
        perror("Error setting SIGCHLD to SIG_IGN.");
        exit(EXIT_FAILURE);
    }

    if((epoll_fd = epoll_create1(EPOLL_CLOEXEC)) == -1) {
        perror("Error creating EPOLL.");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&thread_id, NULL, &epoll_listener, NULL) != 0) {
        perror("Failed creating the pthread. Lack of resources or system limit encountered.");
    }

    DTRACE("%ld:New EPOLL thread: TID=%ld, PID=%ld\n",(long)getppid(),(long)&thread_id,(long)getpid());

    /// Client accept loop.
    ///
    /// Server will create a pthread which sits and listens for new clients and then
    /// runs the initial handshake between client/server.
    while(1) {        
        // Accept a new connection and get socket to use for client:
        client_len = sizeof(client_address);
        if((client_fd = accept(server_sockfd, (struct sockaddr *) &client_address, &client_len)) == -1) {
            perror("Error making connection with the client.");
        }

        // Create a pointer for the file descriptor. Required for a pthread creation.
        client_fd_ptr = (int *) malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        if(pthread_create(&thread_id, NULL, &handle_client, client_fd_ptr)) {
            perror("Error creating the temporary ACCEPT pthread.");
        }

        DTRACE("%ld:New ACCEPT thread: TID=%ld, PID=%ld\n",(long)getppid(),(long)&thread_id,(long)getpid());
    }

    exit(EXIT_SUCCESS);
}

int create_server() {
    int server_sockfd;
    struct sockaddr_in server_address;

    if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error creating socket.");
    }

    int i=1;
    if(setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i))) {
        perror("Error setting sockopt.");
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT);

    if((bind(server_sockfd, (struct sockaddr *) &server_address, sizeof(server_address))) == -1){
        perror("Error assigning address to socket.");
        return -1;
    }

    // Start listening to server socket.
    if((listen(server_sockfd, 10)) == -1){
        perror("Error listening to socket.");
        return -1;
    }

    return server_sockfd;
}

// Epoll loop which listens for any data transfer between the client/server.
void *epoll_listener(void * ignore) {

    struct epoll_event ev_list[MAX_EVENTS];
    int events;
    int i;

    while(1) {
        //while (waitpid(-1,NULL,WNOHANG) > 0);
        events = epoll_pwait(epoll_fd, ev_list, MAX_EVENTS, -1, 0);

        if(events == -1) {
            if(errno == EINTR) {
                continue;
            } else {
                perror("Epoll loop error.");
                exit(EXIT_FAILURE);
            }
        }

        DTRACE("%ld:Sees EVENTS=%d from FD=%d.\n",(long)getppid(), events, ev_list[0].data.fd);        

        for(i = 0; i < events; i++) {
            // If there is an event and the associated file is available for read.
            if(ev_list[i].events & EPOLLIN) {
                if(transfer_data(ev_list[i].data.fd, client_fd_tuples[ev_list[i].data.fd])) {
                    perror("Error reading/writing to the client. Closing shop.\n");
                    kill(bash_fd[ev_list[i].data.fd], SIGTERM);
                    close(ev_list[i].data.fd);
                    close(client_fd_tuples[ev_list[i].data.fd]);
                }
            } else if(ev_list[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
                fprintf(stderr, "Received an EPOLLHUP or EPOLLERR on %d. Shutting it and %d down.\n", ev_list[i].data.fd, client_fd_tuples[ev_list[i].data.fd]);
                kill(bash_fd[ev_list[i].data.fd], SIGTERM);
                close(client_fd_tuples[ev_list[i].data.fd]);
            }
        }
    }
}

// Handles the three-way handshake and starts up the create_processes method.
void  *handle_client(void *client_fd_ptr) {

    // Dereference the int pointer and get rid of the memory now that we no longer need it.
    int client_fd = *(int *) client_fd_ptr;
    free(client_fd_ptr);
    pthread_detach(pthread_self());

    // Conduct the three-way handshake with the client.
    if(handshake(client_fd) == -1) {
        perror("Client failed the handshake.");
        close(client_fd);
        return NULL;
    }

    if(set_nonblocking_fd(client_fd) == -1) {
        perror("Error setting client to non-blocking.");
    }

    if(pty_open(client_fd, &tty) == -1) {
        perror("Failed to open pty and start bash.");
    }

    return NULL;
}

int handshake(int client_fd) {
    
    DTRACE("%ld:Starting handshake with CLIENT=%d.\n",(long)getppid(), client_fd);   

    // Three second timer.
    static struct itimerspec timer;
    timer.it_value.tv_sec = 3;

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
        perror("Error setting up sigaction.");
    }

    // Setup the signal event with the appropriate flags and assign to a 
    // specific thread.
    sig_ev.sigev_value.sival_ptr = &alarm_flag;
    sig_ev._sigev_un._tid = syscall(SYS_gettid);

    if(timer_create(CLOCK_REALTIME, &sig_ev, &timer_id) == -1) {
        perror("Error creating handshake timer.");
    }

    if(timer_settime(timer_id, 0, &timer, NULL) == -1) {
        perror("Error setting handshake timer duration.");
    }

    // TODO: Maybe find a bit better way to address comparing the alarm flag. The way it is now, it will return 
    //        the next section's error (since that is when it is checked after read/write start blocking).
    // Sending the challenge to the client.
    if((alarm_flag || write(client_fd, CHALLENGE, strlen(CHALLENGE))) == -1) {
        perror("Server took too long sending message or write failed.");
        return -1;
    }

    if(alarm_flag || (pass = read_client_message(client_fd)) == NULL) {
        perror("Client took too long sending message or read failed.");
        return -1;
    }

    if(alarm_flag || strcmp(pass, SECRET) != 0) {
        perror("Server took too long comparing the challenge, the compare failed, or invalid secret.");
        write(client_fd, ERROR, strlen(ERROR));
        return -1;
    } else {
        write(client_fd, PROCEED, strlen(PROCEED));
    }

    if(signal(SIGALRM, SIG_IGN) == SIG_ERR) {
        perror("Failed to ignore the handshake signal.");
    }

    if(timer_delete(timer_id) == -1) {
        perror("Failed to delete the handshake timer.");
    }

    DTRACE("%ld:Completed handshake with CLIENT=%d.\n",(long)getppid(), client_fd);  

    return 0;
}

// Creates the master and slave pty.
int pty_open(int client_fd, const struct termios *tty) {
    
    char * pty_slave;
    int pty_master, err;
    struct epoll_event ep_ev[2];
    pid_t b_pid;

    DTRACE("%ld:Opening PTY with CLIENT=%d.\n",(long)getppid(), client_fd);  

    /* Open an unused pty dev and store the fd for later reference.
        O_RDWR := Open pty for reading + writing. 
        O_NOCTTY := Don't make it a controlling terminal for the process. 
    */
    if((pty_master = posix_openpt(O_RDWR|O_NOCTTY|O_CLOEXEC)) == -1) {
        perror("Failed openpt.");
        return -1;        
    }

    /// Set pty master fd so it gets closed when bash execs.
    fcntl(pty_master,F_SETFD,FD_CLOEXEC);

    if(set_nonblocking_fd(pty_master) == -1) {
        perror("Error setting client to non-blocking.");
    }
    
    /* Attempt to kickstart the master.
        Grantpt := Creates a child process that executes a set-user-ID-root program changing ownership of the slave 
            to be the same as the effective user ID of the calling process and changes permissions so the owner 
            has R/W permissions.
        Unlockpt := Removes the internal lock placed on the slave corresponding to the pty. (This must be after grantpt).
        ptsname := Returns the name of the pty slave corresponding to the pty master referred to by pty_master. (/dev/pts/nn).
    */
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
        DTRACE("%ld:PTY_MASTER=%i and PTY_SLAVE=%s.\n",(long)getppid(), pty_master, pty_slave);  
        
        // pty_master is not needed in the child, close it.
        close(pty_master);
        close(client_fd);
        if(create_bash_process(pty_slave, tty) == -1) {
            perror("Failed to create bash process.");
        }

        // Should never reach this point since the bash process will just terminate.
        exit(EXIT_FAILURE);
    }

    DTRACE("%ld:SLAVE_PID=%d.\n",(long)getppid(), (int)b_pid);  
    free(pty_slave);

    client_fd_tuples[client_fd] = pty_master;
    client_fd_tuples[pty_master] = client_fd;

    bash_fd[client_fd] = b_pid;
    bash_fd[pty_master] = b_pid;

    // Setup the epoll event handling (R/W).
    ep_ev[0].data.fd = client_fd;
    ep_ev[0].events = EPOLLIN | EPOLLET;

    ep_ev[1].data.fd = pty_master;
    ep_ev[1].events = EPOLLIN | EPOLLET;

    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, ep_ev) == -1) {
        perror("Error creating epoll_ctl for the client_fd.");
        return -1;
    }

    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pty_master, ep_ev + 1) == -1) {
        perror("Error creating epoll_ctl for the pty_master.");
        return -1;
    }
    
    return 0;
}

int create_bash_process(char *pty_slave, const struct termios *tty) {

    int pty_slave_fd;

    if(setsid() == -1) {
        perror("Could not create a new session.");
        return -1;            
    }

    if((pty_slave_fd = open(pty_slave, O_RDWR)) == -1) {
        perror("Failed opening PTY_SLAVE.");
        return -1;
    }

    DTRACE("%ld:Creating bash and connecting it to SLAVE_FD=%i.\n",(long)getppid(), pty_slave_fd); 
    
    if(tcsetattr(pty_slave_fd, TCSANOW, tty) == -1) {
        perror("Could not set the set the terminal parameters.");
        return -1;
    }

    if ((dup2(pty_slave_fd,0) == -1) || (dup2(pty_slave_fd,1) == -1) || (dup2(pty_slave_fd,2) == -1)) {
        perror("dup2() call for FD 0, 1, or 2 failed");
        exit(EXIT_FAILURE); 
    }

    close(pty_slave_fd);
    free(pty_slave);
    execlp("bash", "bash", NULL);

    perror("Failed to exec bash!");

    return -1;
}

int set_nonblocking_fd(int fd) {
    
    int fd_flags;

    // Get the current fd flags.
    if((fd_flags = fcntl(fd, F_GETFL, 0)) == -1) {
        perror("Error getting fd_flags.");
        return -1;
    }

    // Add the non-blocking flag.
    fd_flags |= O_NONBLOCK;

    // Set the new flag set for the fd.
    if(fcntl(fd, F_SETFL, fd_flags) == -1) {
        perror("Error setting fd_flags.");
        return -1;
    }

    return 0;
}

// Signal handler for the handshake.
void sighandshake_handler(int signal, siginfo_t * sip, void * ignore)
{
    fprintf(stdout, "Alarm has a value: %d\n", *(int *) (sip->si_ptr));
    *(int *) sip->si_ptr = 1;
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
            perror("Client closed connection unexpectedly\n");
            
        return NULL; 
    }

  msg[nread] = '\0';

  return msg;
}

int transfer_data(int from, int to) {
    
    char buf[MAX_LENGTH];
    ssize_t nread, nwrite;

    while((nread = read(from, buf, MAX_LENGTH)) > 0) {
        if((nwrite = write(to, buf, nread) == -1)) {
            perror("Failed writing data.");
            break;
        }
    }

    if (nread == -1) DTRACE("%ld:Error read()'ing from FD %d\n",(long)getpid(),from);
    if (nwrite == -1) DTRACE("%ld:Error write()'ing to FD %d\n",(long)getpid(),to);

    if(nwrite == -1 && errno == EPIPE) {
        perror("I SEE EPIPE ERROR");
        return -1;
    }

    if(nread == -1 && errno != EWOULDBLOCK && errno != EAGAIN) {
        perror("Failed reading data.");
        return -1;
    }

    if(nread == 0) {
        DTRACE("%ld:READ NREAD=0\n",(long)getpid());
        return -1;
    }

    return 0;
}
