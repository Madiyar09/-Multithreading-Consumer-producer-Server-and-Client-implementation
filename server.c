#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <semaphore.h>
#include "passivesock.c"

#define THREADS 1024

int BUFSIZE, cnt;

typedef struct item
{
	char *name;
	int size;
} item_t;

item_t *buf;

sem_t mutex, full, empty;

int passivesock( char *service, char *protocol, int qlen, int *rport );

int writeOK(int ssock) {
	char *buf = "OK";
	if(write(ssock, buf, 2) < 0) {
		close(ssock);
		return 0;
	}
	return 1;
}

void writeBAD(int ssock) {
	char *buf = "BAD";
	write(ssock, buf, 3);
	close(ssock);
}

// transforms size/string to string and checks format errors
char *getString(char * cur) {
	int pos = 0;
	int size = 0;
	while(cur[pos] >= '0' && cur[pos] <= '9') {
		size = size * 10 + cur[pos] - '0';
		pos ++;
	}
	
	if(cur[pos] != '/' || size == 0 || (strlen(cur) - pos - 1) != size)
		return NULL;
	
	pos ++;
	char * temp = (char *) malloc(sizeof(char) * (size + 1));
	
	int cnt = 0;
	while(cnt < size) 
		temp[cnt ++] = cur[pos ++];
	temp[cnt] = '\0';
	
	return temp;
}

// size: int to string
char *getSize(int x) {
	char *temp = malloc(sizeof(char) * 10);
	int cnt = 0;
	
	while(x) {
		temp[cnt] = x % 10 + '0';
		x /= 10;
		cnt ++;
	}
	
	char *size = malloc(sizeof(char) * cnt);
	for(int i = 0; i < cnt; i ++)
		size[i] = temp[cnt - i - 1];
	size[cnt] = '\0';
	
	free(temp);
	
	return size;
}

void *connClient(void *arg) {
	char * cur = (char *) malloc(sizeof(char) * 1024);
	int cc;
	int ssock = (intptr_t) arg;
	int producer = 0;	// variable to remember it is producer
	
	//printf( "A client has arrived.\n" );
	//fflush( stdout );

	/* start working for this guy */
	for(;;) {
		if ( (cc = read( ssock, cur, 1024 )) <= 0 ) {
			printf( "The client has gone.\n" );
			close(ssock);
			break;
		} else {
			cur[cc] = '\0';
			if(strcmp(cur, "producer") == 0) {		// handle consumer
				sem_wait( &empty );
				sem_wait( &mutex );
				
				if(writeOK(ssock) == 0)
					break;
					
				producer = 1;
			}
			else if(strcmp(cur, "consumer") == 0) {		//handle consumer
				sem_wait( &full );
				sem_wait( &mutex );
				
				cnt --;
				// transform string in buffer to size/string format
				char *size = getSize(buf[cnt].size);
				char *temp = malloc(sizeof(char) * (strlen(size) + strlen(buf[cnt].name) + 1));
				strcat(temp, size);
				strcat(temp, "/");
				strcat(temp, buf[cnt].name);
				write(ssock, temp, strlen(temp));
				free(buf[cnt].name);
				
				free(size);
				free(temp);
				
				sem_post( &mutex );
				sem_post( &empty );
				
				close(ssock);
			} else {		// handle size/string format
				char * temp = getString(cur);
				
				if(temp == NULL || producer == 0) {
					writeBAD(ssock);
					break;
				}
				printf("%s\n", temp);
				
				buf[cnt].name = (char *) malloc(strlen(temp) + 1);
				strcpy(buf[cnt].name, temp);
				buf[cnt].size = strlen(temp);
				cnt ++;				
				
				sem_post( &mutex );
				sem_post( &full );
				
				free(temp);
				
				writeOK(ssock);
				close(ssock);
				break;
			}
		}
	}
	
	free(cur);
	
	pthread_exit( NULL );
}

int main( int argc, char *argv[] )
{
	char		*service;
	pthread_t 	threads[THREADS];
	struct sockaddr_in	fsin;
	int		cnt = 0;
	int		alen;
	int		msock;
	int		rport = 0;
	int 	status;
	
	switch (argc) 
	{
		case	3:
			// No args? let the OS choose a port and tell the user
			BUFSIZE = atoi(argv[2]);
			rport = 1;
			break;
		case	4:
			// User provides a port? then use it
			BUFSIZE = atoi(argv[2]);
			service = argv[3];
			break;
		default:
			fprintf( stderr, "usage: prodcon_server bufsize [port]\n" );
			exit(-1);
	}
	
	sem_init( &mutex, 0, 1 );
	sem_init( &full, 0, 0 );
	sem_init( &empty, 0, BUFSIZE );
	
	buf = (item_t *) malloc(BUFSIZE * sizeof(item_t));

	msock = passivesock( service, "tcp", 1, &rport );
	if (rport)
	{
		//	Tell the user the selected port
		printf( "server: port %d\n", rport );	
		fflush( stdout );
	}
	
	
	for (;;)
	{
		int ssock;

		alen = sizeof(fsin);
		ssock = accept( msock, (struct sockaddr *)&fsin, &alen );
		if (ssock < 0)
		{
			fprintf( stderr, "accept: %s\n", strerror(errno) );
			exit( -1 );
		}
		
		// create thread for each connection
		status = pthread_create( &threads[cnt], NULL, connClient, (void *) (intptr_t) ssock );
		if ( status != 0 )
		{
			printf( "pthread_create error %d.\n", status );
			exit( -1 );
		}
		cnt ++;
		
	}
	
	for(int i = 0; i < cnt; i ++)
		pthread_join(threads[i], NULL);
}
