#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <termios.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>


#define DEVICE      "/dev/ttyUSB0"
#define SHM_SIZE    42
#define SHM_KEY     5515189


//global flag to handle loop exit with CTRL+C (SIGINT)
short stop;
short debug;
static char data[40]; //holds current values read from JeeLink

void error(const char*, ...);
void sig_handler(int);
void setup_port(int fd);


int main(int argc, char *argv[]) {
    int result = 0;
    int sresult = 0;
    char *shm_mem;
    int shm_id;

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided as argument!\n");
        fprintf(stderr,"Usage: %s port [debug]\n", argv[0]);
        fprintf(stderr,"\tport - port numer, ex. 2200\n");
        fprintf(stderr,"\tdebug - optional debug mode\n");
        fprintf(stderr,"Example: %s 2200 debug\n", argv[0]);
        return 1;
    }

    //do some init
    bzero(data, sizeof(data));
    stop = 0;
    //register handler for CTRL+C
    signal(SIGINT, sig_handler);
    //check if debug mode
    if ((argc > 2) && (strcmp(argv[2], "debug") == 0)) {
        debug = 1;
    } else {
        debug = 0;
    }

    //shm memory init
    /*
    shm_id = init_shm(shm_mem);
    */

    //int shmid = -1;
    key_t key = SHM_KEY;
    //char *shm;

    //create the segment
    if ((shm_id = shmget(key, SHM_SIZE, IPC_CREAT | 0666)) < 0) {
        error("ERROR creating shared mem segment! ", strerror(errno));
    }
    //now we attach the segment to our data space.
    if ((shm_mem = shmat(shm_id, NULL, 0)) == (char *) -1) {
        error("ERROR attaching shared mem segment! ", strerror(errno));
    }
    if (debug) { printf("Shared memory created, id is: %d\n", shm_id); }

    /*
    char *s = shm_mem;
    *s = 'X';
    *s++;
    *s = '\0';
    printf("1: %s\n", shm_mem);
    printf("2: %s\n", s);
    */

    switch(fork())
    {
      case -1:
        //parent code - fork is unsuccessful
        error("ERROR in fork! ", strerror(errno));
        break;
      case 0:
        //child code here - serial port reading
        result = handle_serial_port(shm_mem);
        stop = 1;
        break;
      default:
        //parent code here - TCP server
        sresult = listen_server(argc, argv, shm_mem);
        stop = 1;
        wait();
        close_shm(shm_id, shm_mem);
        if (debug) { printf("Exiting with code=%d\n", (result && sresult)); }
    }

    return (result && sresult);
}


//create shared memory segment
int init_shm(char *shm) {
    int shmid = -1;
    key_t key = SHM_KEY;
    //char *shm;

    //create the segment
    if ((shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666)) < 0) {
        error("ERROR creating shared mem segment! ", strerror(errno));
    }
    //now we attach the segment to our data space.
    if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
        error("ERROR attaching shared mem segment! ", strerror(errno));
    }
    if (debug) { printf("Shared memory created, id is: %d\n", shmid); }

    return shmid;
}

//destroy shared memory segment
int close_shm(int shmid, char *shm) {
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    if (debug) { printf("Shared memory destroyed for id: %d\n", shmid); }
}

//TCP server
int listen_server(int argc, char *argv[], char *shm_mem)
{
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    stop = 0;


    if (debug) { printf("Starting server...\n"); }

    //create serwer socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        close(sockfd);
        error("ERROR creating socket! ", strerror(errno));
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    //set port number
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    //bind into socket
    int on = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0) {
        close(sockfd);
        error("ERROR setting socket options! ", strerror(errno));
    }
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        error("ERROR on binding socket! ", strerror(errno));
    }

    //listen for connections
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    while (stop == 0) {
        if (debug) { printf("Ready to serve, waiting for incoming connection...\n"); }
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) {
            fprintf(stderr, "ERROR on accepting incoming connection! %s", strerror(errno));
        } else {
            if (debug) { printf("Incoming connection accepted, sending data...\n"); }
            //put data into socket
            n = write(newsockfd, shm_mem, 33);
            if (n < 0) { fprintf(stderr, "ERROR writing to socket! %s", strerror(errno)); }
            //closing socket
            shutdown(newsockfd, 2);
            close(newsockfd);
            if (debug) { printf("Done, data sent.\n"); }
        }
    }

    shutdown(sockfd, 2);
    close(sockfd);
    if (debug) { printf("TCP server shutdown completed.\n"); }
    return 0;
}

//prints fatal error message and exits program
void error(const char *msg, ...)
{
    perror(msg);
    _exit(1);
}

//signal handler
void sig_handler(int signum)
{
    if (debug) { printf("Received signal: %d\n", signum); }
    if (signum == SIGINT) {
        stop = 1;
        if (debug) { printf("Signal is SIGINT, exiting after next request. It can take few seconds, please wait...\n"); }
    } else {
        exit(2);
    }
}


int handle_serial_port(char *shm_mem) {
	int fd;
	int flag;
	int read_len;
	char buf[16];
    struct tm *local;
    time_t t;
    char tbuffer[25];

    //open serial device
	fd = open(DEVICE, O_RDONLY | O_NOCTTY | O_NDELAY);
	if (fd < 0) {
	    fprintf(stderr, "ERROR opening %s serial device! ", DEVICE);
		return 1;
	}

	flag = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flag & ~O_NDELAY);

    setup_port(fd);

    //start reading from serial port
	while (stop == 0) {
	    //get current time
        bzero(tbuffer, sizeof(tbuffer));
        t = time(NULL);
        local = localtime(&t);
        strftime (tbuffer, 25, "%Y-%m-%d %H:%M:%S", local);

		bzero(buf, sizeof(buf));
		read_len = 0;
		//read from serial device
		read_len = read(fd, buf, sizeof(buf));
        bzero(data, sizeof(data));
        strcat(data, tbuffer);
        strcat(data, " ");
        strcat(data, buf);

        if (debug) { printf("Got data from JeeLink: %s", data); }

        //set current values into shared mem

        char *s = shm_mem;
        int i;
        for (i=0; i<34; i++) {
            *s = data[i];
            *s++;
        }
        *s = '\0';

        if (debug) { printf("Data saved into shared memory.\n"); }

	}

	close(fd);
    if (debug) { printf("Serial port reading thread - shut down completed.\n"); }

	return 0;
}

//setup serial port for reading from JeeLink
void setup_port(int fd) {
	struct termios opt, old;
	int baudrate;

	//set baudrate to 0 (terminating connection) and go back to normal
	if (debug) { printf("Resetting baudrate to 0\n"); }
	tcgetattr(fd, &opt);
	tcgetattr(fd, &old);
	cfsetospeed(&opt, B0);
	cfsetispeed(&opt, B0);
	tcsetattr(fd, TCSANOW, &opt);
	sleep(1);
	tcsetattr(fd, TCSANOW, &old);

	if (debug) { printf("Setting baudrate to 57600\n"); }
	tcgetattr(fd, &opt);
	baudrate = B57600;
	cfsetospeed(&opt, baudrate);
	cfsetispeed(&opt, baudrate);

	opt.c_cflag = (opt.c_cflag & ~CSIZE) | CS8;

	//set into raw, no echo mode
	opt.c_iflag = IGNBRK;
	opt.c_lflag = 0;
	opt.c_oflag = 0;
	opt.c_cflag |= CLOCAL | CREAD;

	opt.c_cflag &= ~CRTSCTS;
	opt.c_cc[VMIN] = 13;
	opt.c_cc[VTIME] = 0;

	//turn off software control
	opt.c_iflag &= ~(IXON | IXOFF | IXANY);
	opt.c_iflag |= (IXON | IXOFF | IXANY);

	//no parity
	opt.c_cflag &= ~(PARENB | PARODD);

	//1 stopbit
	opt.c_cflag &= ~CSTOPB;

    //enable canonical output
    opt.c_lflag |= (ICANON | ECHO | ECHOE);

	tcsetattr(fd, TCSANOW, &opt);
}

