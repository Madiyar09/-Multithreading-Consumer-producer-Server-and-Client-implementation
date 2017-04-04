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
#include "connectsock.c"

#define BUFSIZE		4096
#define THREADS		1024
#define PRODUCER_STRING "12/Hello World!"

char	*service;		
char	*host = "localhost";

int connectsock( char *host, char *service, char *protocol );

// transform size/string to string
char *getString(char * cur) {
	int pos = 0;
	int size = 0;
	while(cur[pos] >= '0' && cur[pos] <= '9') {
		size = size * 10 + cur[pos] - '0';
		pos ++;
	}
	pos ++;
	
	char * temp = (char *) malloc(sizeof(char) * (size + 1));
	
	int cnt = 0;
	while(cnt < size) {
		temp[cnt ++] = cur[pos ++];
	}
	temp[cnt] = '\0';
	
	return temp;
}

void *consumer(void *arg) {
	int cc, csock;
	
	/*	Create the socket to the controller  */
	if ( ( csock = connectsock( host, service, "tcp" )) == 0 )
	{
		fprintf( stderr, "Cannot connect to server.\n" );
		exit( -1 );
	}
	
	// tell server that it is consumer
	char *buf = "consumer";
	char *cur = (char *) malloc(sizeof(char) * 1024);
	
	write(csock, buf, strlen(buf));
	
	for(;;) {
		if((cc = read( csock, cur, 1024 )) <= 0) {
			printf( "The server has gone.\n" );
			close(csock);
			break;
		} else {
			cur[cc] = '\0';
		
			if(strcmp(cur, "BAD") == 0)
				printf("BAD\n");
			else {
				// getting the string
				buf = getString(cur);
				printf("%s\n", buf);
			}
			break;
		}
	}
	
	close(csock);

	pthread_exit( NULL );
}

void *producer (void *arg) {
	int cc, csock;
	
	/*	Create the socket to the controller  */
	if ( ( csock = connectsock( host, service, "tcp" )) == 0 )
	{
		fprintf( stderr, "Cannot connect to server.\n" );
		exit( -1 );
	}
	
	// tell server that it is producer
	char *buf = "producer";
	char *cur = (char *) malloc(sizeof(char) * 1024);
	
	write(csock, buf, strlen(buf));
	
	// receive respond from server
	if((cc = read( csock, cur, 1024 )) <= 0) {
		printf( "The server has gone.\n" );
		close(csock);
	} else {
		cur[cc] = '\0';
		if(strcmp(cur, "BAD") == 0) { 
			printf("%s\n", cur);
			close(csock);
			exit(-1);
		}
	}
	
	// send string to the server
	buf = PRODUCER_STRING;
	write(csock, buf, strlen(buf));
	
	if((cc = read( csock, cur, 1024 )) <= 0) {
		printf( "The server has gone.\n" );
		close(csock);
	} else {
		cur[cc] = '\0';
		if(strcmp(cur, "BAD") == 0) { 
			printf("%s\n", cur);			
		}
	}
	
	close(csock);
	
	pthread_exit( NULL );
}

int main( int argc, char *argv[] )
{
	pthread_t threads[THREADS];
	char	buf[BUFSIZE];
	char	*type;
	int		status;
	int		cc;
	int		csock;
	int		num;
	int 	cnt = 0;
	
	switch( argc ) 
	{
		case    4:
			type = argv[1];
			service = argv[2];
			num = atoi(argv[3]);
			break;
		case    5:
			type = argv[1];
			host = argv[2];
			service = argv[3];
			num = atoi(argv[4]);
			break;
		default:
			fprintf( stderr, "usage: prodcon_producer/prodcon_consumer [host] port prod_num\n" );
			exit(-1);
	}
	
	for(int i = 0; i < num; i ++) {
		if(strcmp(type, "prodcon_consumer") == 0) {
			status = pthread_create( &threads[cnt], NULL, consumer, NULL );
		} else if(strcmp(type, "prodcon_producer") == 0) {
			status = pthread_create( &threads[cnt], NULL, producer, NULL );
		} else {
			fprintf( stderr, "usage: prodcon_producer/prodcon_consumer [host] port prod_num\n" );
			exit(-1);
		}
		
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


