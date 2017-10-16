/** Lab3: Server 10/16/2017 for CS 591.
 * 
 * Author: Taylor Flatt
 * 
 * Properties:
 *   -- parallel/concurrent server.
 *   -- uses epoll in order to handle read/writes.
 *   -- uses two pthreads in order to handle clients.
 *           (1) conducts the handshake and verification and is temporary.
 *           (2) conducts the reading/writing between all clients and is permanent (epoll).
 *   -- uses read()/write() for all I/O.
 *   -- forces reads/writes to be in non-blocking mode.
 *   -- sets SIGCHLD to be ignored in server to avoid having to collect client handling subprocesses.
 *   -- handles broken/malicious clients in the handshake process (timeout).
 * 
 * Remarks:
 *   -- In order to compile, gcc requires -pthread for pthreads and -lrt for timers.
 * 
 * Usage: server
*/

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

/* Preprocessor constants. */
#define PORT 4070
#define MAX_LENGTH 4096
#define MAX_NUM_CLIENTS 64000
#define MAX_EVENTS 24
#define SECRET "cs407rembash\n"
#define CHALLENGE "<rembash>\n"
#define PROCEED "<ok>\n"
#define ERROR "<error>\n"

/* Prototypes. */
int create_server();
void *epoll_listener(void * ignore);
void *handle_client(void *client_fd_ptr);
int handshake(int client_fd);
int pty_open(int client_fd);
int create_bash_process(char *pty_slave);
int set_nonblocking_fd(int fd);
void sighandshake_handler(int signal, siginfo_t * sip, void * ignore);
char *read_client_message(int client_fd);
int transfer_data(int from, int to);

/* Global Variables */
/* A map to store the fd for pty/socket and bash_fd. */
int client_fd_tuples[MAX_NUM_CLIENTS * 2 + 5];
pid_t bash_fd[MAX_NUM_CLIENTS * 2 + 5];
int epoll_fd;

int main(int argc, char *argv[]) {
    int client_fd, server_sockfd;
    int *client_fd_ptr;
    pthread_t thread_id;

    if((server_sockfd = create_server()) == -1) {
        perror("Error creating the server.");
        exit(EXIT_FAILURE);
    }

    /* Forces writes to closed sockets to return an error rather than a signal. */
    if(signal(SIGPIPE,SIG_IGN) == SIG_ERR) {
        perror("Error setting SIGPIPE to SIG_IGN.");
        exit(EXIT_FAILURE);
    }

    /* Forces child processes to be automatically discarded when they terminate. */
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

    /* CLIENT ACCEPT LOOP */
    while(1) {

        if((client_fd = accept(server_sockfd, (struct sockaddr *) NULL, NULL)) == -1) {
            perror("Error making a connection with the client.");
        }

        /* Create a pointer for the fd. Required for a pthread creation. */
        client_fd_ptr = (int *) malloc(sizeof(int));
        *client_fd_ptr = client_fd;
        if(pthread_create(&thread_id, NULL, &handle_client, client_fd_ptr)) {
            perror("Error creating the temporary ACCEPT pthread.");
        }

        DTRACE("%ld:New ACCEPT thread: TID=%ld, PID=%ld\n",(long)getppid(),(long)&thread_id,(long)getpid());
    }

    exit(EXIT_SUCCESS);
}

/** Creates the server by setting up the socket and begins listening.
 * 
 * Returns: An integer corresponding to server's file descriptor on success or failure -1.
*/
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

    /* Name the socket and set the port. */
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT);

    if((bind(server_sockfd, (struct sockaddr *) &server_address, sizeof(server_address))) == -1){
        perror("Error assigning address to socket.");
        return -1;
    }

    if((listen(server_sockfd, 10)) == -1){
        perror("Error listening to socket.");
        return -1;
    }

    return server_sockfd;
}

// Epoll loop which listens for any data transfer between the client/server.
/** An epoll listener which handles communication between the client and server by 
 * waiting for read/write events to come through on available file descriptors.
 * 
 * ignore: A pointer which is a required argument. It is not used.
 * 
 * Returns: None.
*/
void *epoll_listener(void * ignore) {

    struct epoll_event ev_list[MAX_EVENTS];
    int events;
    int i;

    while(1) {
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
            /* Check if there is an event and the associated fd is available for reading. */
            if(ev_list[i].events & EPOLLIN) {
                DTRACE("%ld:Starting data transfer PTY-->socket (FD %d-->%d)\n",(long)getpid(),ev_list[i].data.fd, client_fd_tuples[ev_list[i].data.fd]);
                if(transfer_data(ev_list[i].data.fd, client_fd_tuples[ev_list[i].data.fd])) {
                    perror("Error reading/writing to the client. Closing shop.\n");
                    //kill(bash_fd[ev_list[i].data.fd], SIGTERM);
                    close(ev_list[i].data.fd);
                    close(client_fd_tuples[ev_list[i].data.fd]);
                }
                DTRACE("%ld:Completed data transfer PTY-->socket\n",(long)getpid());
            } else if(ev_list[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
                fprintf(stderr, "Received an EPOLLHUP or EPOLLERR on %d. Shutting it and %d down.\n", ev_list[i].data.fd, client_fd_tuples[ev_list[i].data.fd]);
                //kill(bash_fd[ev_list[i].data.fd], SIGTERM);
                close(client_fd_tuples[ev_list[i].data.fd]);
            }
        }
    }
}

// Handles the three-way handshake and starts up the create_processes method.
/** Handles a client attempting to connect by conducting the handshake, setting the 
 * nonblocking parameter, and opening the pty.
 * 
 * client_fd_ptr: A pointer corresponding to the client's file descriptor (int).
 * 
 * Returns: None.
*/
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

    if(pty_open(client_fd) == -1) {
        perror("Failed to open pty and start bash.");
    }

    return NULL;
}

/** Conducts a three-way handshake with the client to verify that it is 
 * authorized to connect. The handshake will timeout if it takes too long 
 * to authenticate the client.
 * 
 * client_fd: An integer corresponding to the clients's file descriptor.
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
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

    int alarm = 0;
    char *pass;
    timer_t timer_id;

    sigemptyset(&sig_act.sa_mask);

    if(sigaction(SIGALRM, &sig_act, NULL) == -1) {
        perror("Error setting up sigaction.");
    }

    // Setup the signal event with the appropriate flags and assign to a 
    // specific thread.
    sig_ev.sigev_value.sival_ptr = &alarm;
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
    if((alarm || write(client_fd, CHALLENGE, strlen(CHALLENGE))) == -1) {
        perror("Server took too long sending message or write failed.");
        return -1;
    }

    if(alarm || (pass = read_client_message(client_fd)) == NULL) {
        perror("Client took too long sending message or read failed.");
        return -1;
    }

    if(alarm || strcmp(pass, SECRET) != 0) {
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

/** Opens the PTY and creates the connection between bash and the client by forking off a subprocess 
 * to run bash.
 * 
 * client_fd: An integer corresponding to the client's file descriptor.
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
int pty_open(int client_fd) {
    
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
        if(create_bash_process(pty_slave) == -1) {
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

/** Sets up a PTY for the bash process and redirects stdin, stdout, and stderr to the fd.
 * 
 * pty_slave: A string corresponding to a subprocess slave pty name.
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
int create_bash_process(char *pty_slave) {

    int pty_slave_fd;

    if(setsid() == -1) {
        perror("Could not create a new session.");
        return -1;            
    }

    // Setup the pty for the bash subprocess.
    if((pty_slave_fd = open(pty_slave, O_RDWR|O_NOCTTY|O_CLOEXEC)) == -1) {
        perror("Failed opening PTY_SLAVE.");
        return -1;
    }

    DTRACE("%ld:Creating bash and connecting it to SLAVE_FD=%i.\n",(long)getppid(), pty_slave_fd); 
    
    if ((dup2(pty_slave_fd, STDIN_FILENO) == -1) || (dup2(pty_slave_fd, STDOUT_FILENO) == -1) || (dup2(pty_slave_fd, STDERR_FILENO) == -1)) {
        perror("dup2() call for FD 0, 1, or 2 failed");
        exit(EXIT_FAILURE); 
    }

    close(pty_slave_fd);
    free(pty_slave);
    execlp("bash", "bash", NULL);

    DTRACE("%ld:Failed to exec bash on SLAVE_FD=%i.\n",(long)getppid(), pty_slave_fd); 

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

/** Timer handler for the handshake. Sets a flag to 1 if it has executed.
 * 
 * signal: Unused.
 * sip: Contains singal information such as the signal number, value, etc. Used to 
 *      set the appropriate flag so it can be seen elsewhere.
 * ignore: Unused.
 * 
 * Returns: None.
*/
void sighandshake_handler(int signal, siginfo_t * sip, void * ignore)
{
    fprintf(stdout, "Alarm has a value: %d\n", *(int *) (sip->si_ptr));
    *(int *) sip->si_ptr = 1;
}

/** Reads the handshake messages.
 * 
 * client_fd: An integer corresponding to the clients's file descriptor.
 * 
 * Returns: A null terminated string if successful, otherwise NULL if an error is
 *          encountered.
*/
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

/** Actually reads and writes data to and from sockets.
 * 
 * Remarks: Need to check for both since either can be set...
 * EWOULDBLOCK/EAGAIN: If a read is going to block.
 * 
 * from: Integer representing the source file descriptor (read).
 * to: Integer representing the targer file descriptor (write).
 * 
 * Returns: An integer corresponding to the success 0, or failure -1.
*/
int transfer_data(int from, int to) {
    
    char buf[MAX_LENGTH];
    ssize_t nread, nwrite;

    while((nread = read(from, buf, MAX_LENGTH)) > 0) {
        if((nwrite = write(to, buf, nread) == -1)) {
            perror("Failed writing data.");
            break;
        }
    }

    if(nread == -1 && errno != EWOULDBLOCK && errno != EAGAIN) {
        DTRACE("%ld:Error read()'ing from FD %d\n",(long)getpid(),from);
        perror("Failed reading data.");
        return -1;
    }
    /*
    if(nwrite == -1 && errno == EPIPE) {
        DTRACE("%ld:Error write()'ing to FD %d\n",(long)getpid(),to);
        return -1;
    }
    */
    if(nread == 0) {
        DTRACE("%ld:NREAD=0 The socket was closed.\n",(long)getpid());
        return -1;
    }

    return 0;
}
