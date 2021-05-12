#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <errno.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/sendfile.h>
#include <sys/stat.h>

#define FILE "/tmp/bigfile"
#define MAX(a,b) ((a>b?a:b))

int main( int argc, const char *argv[], const char *envp[] )
{
	int outfd, infd, nfds;
	struct sockaddr_in addr;
	int flags;
	struct sockaddr_in inaddr;
	socklen_t addrlen = sizeof(inaddr);
	int datasock;
	char buf[1024];
	fd_set rfds, wfds;
	struct stat statbuf;

	if( argc <= 1 || ! argv || !argv[1] || strlen(argv[1]) <= 0 || (outfd = open(argv[1], O_RDONLY|O_NONBLOCK)) == -1 ) {
		fprintf(stderr, "Please pass the file to serve on the command line.\n");
		return 1;
	}

	if( fstat(outfd, &statbuf) == -1 ) {
		fprintf(stderr, "Could not stat file descriptor: (%s)\n", strerror(errno));
		close(outfd);
		return -1;
	}
	printf("File \"%s\", size: %lu bytes opened.\n", FILE, statbuf.st_size);

	if( (infd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
		fprintf(stderr, "Could not create listening socket: (%s)\n", strerror(errno));
		close(outfd);
		return 1;
	}

	/* Make infd non-block */
	flags = fcntl(infd,F_GETFL);
	assert(flags != -1);
	if( fcntl(infd, F_SETFL, flags | O_NONBLOCK) == -1 ) {
		fprintf(stderr, "Unable to set nonblock on listening socket: (%s)\n", strerror(errno));
		close(infd);
		close(outfd);
		return 1;
	}

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(4443);

	if( bind(infd, (struct sockaddr *)&addr, sizeof(addr)) == -1 ) {
		fprintf(stderr, "Could not bind socket for listening: (%s)\n", strerror(errno));
		close(outfd);
		return 1;
	}

	printf( "Bound to port %d, entering listening loop.\n", ntohs(addr.sin_port));

	if( listen(infd, 1) != 0 ) {
		fprintf(stderr, "Could not listen for incomming connections: (%s)\n", strerror(errno));
		close(infd);
		close(outfd);
		return 1;
	}
	printf( "Listening for incomming connections.\n");

	while( 1 ) {
		int ret;
		off_t totsent;

		FD_ZERO(&rfds); FD_ZERO(&wfds);
		FD_SET(infd, &rfds);
		nfds=0;
		nfds = MAX(nfds,infd);
		

		ret = select(nfds+1, &rfds, &wfds, NULL, NULL);

		if( ret == -1 ) {
			fprintf(stderr, "Error selecting socket for listening: (%s)\n", strerror(errno));
			close(infd);
			close(outfd);
			return -1;
		} else if (FD_ISSET(infd, &rfds)) {
			if( (datasock = accept(infd, (struct sockaddr *)&inaddr, &addrlen)) == -1 ) {
				fprintf(stderr, "Unable to accept incomming connection: (%s)\n", strerror(errno));
				close(infd);
				close(outfd);
				return 1;
			}

			// make new socket non-block if it isn't already	
			flags = fcntl(datasock,F_GETFL);
			assert(flags != -1);
			if( fcntl(datasock, F_SETFL, flags | O_NONBLOCK) == -1 ) {
				fprintf(stderr, "Unable to set nonblock on listening socket: (%s)\n", strerror(errno));
				close(datasock);
				close(infd);
				close(outfd);
				return 1;
			}

			if( inet_ntop(AF_INET, (void *)&inaddr.sin_addr, buf, 1024) != buf )
				printf("Connection from UNKNOWN\n" );
			else
				printf( "Connection from %s:%d\n", buf, ntohs(inaddr.sin_port));

			totsent = 0;

			while( totsent < statbuf.st_size ) {
				ssize_t sent = 0;

				FD_ZERO(&wfds);
				FD_SET(datasock, &wfds);
				nfds=0;
				nfds = MAX(nfds, datasock);

				ret = select(nfds+1, NULL, &wfds, NULL, NULL);
				if( ret == 1 && FD_ISSET(datasock, &wfds) ) {
					sent = sendfile(datasock, outfd, &totsent, statbuf.st_size);
					if( sent == -1 ) {
						printf("Sending aborted by far end.\n");
						break;
					}
					printf("Sendfile returned having sent %lu (%lu total) of %lu\n", sent, totsent, statbuf.st_size);
				}
			}
			printf("Done sending file.\n");

			close(datasock);

			printf("\nWaiting for new connection.\n");
		} else {
			fprintf(stderr, "BUG BUG BUG!\n");
			close(infd);
			close(outfd);
			return -1;
		}
		
	}			
			
	close(datasock);
	close(infd);
	close(outfd);

	return 0;
}
