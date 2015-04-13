#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>


#define d 1
#define MAX_URL 2048
#define MAX_HTTP_HEADER 8000
#define MAX_HTTP_MESSAGE 800000
#define MAX_HTTP_RESPONSE (MAX_HTTP_MESSAGE + MAX_HTTP_HEADER)
#define MAX_HTTP_REQUEST MAX_HTTP_HEADER
#define IP_LENGTH 32


int cacheSize;

// struct connection_object{
// 	int socket;
// 	int 
// };

struct cache_object{
	struct cach_object *next;
	char url[MAX_URL];
	struct sockaddr_in ip;
	char * data;
	int size;
};
struct cache_object *cache;

int main(int argc, char ** argv){
	if(argc < 3){
		printf("Command should be: myprog <port> <sizeOfCach> (in MBs)\n");
		return 1;
	}


	int port = atoi(argv[1]);
	cacheSize = atoi(argv[2]);
	printf("port: %d\n", port);

	if (port < 1024 || port > 65535) {
		printf("Port number should be equal to or larger than 1024 and smaller than 65535\n");
		return 1;
	}


	if(cacheSize<0){
		printf("Bad cache size entered!\n");
		return 1;
	}
	cache = NULL;
	// cache->next=NULL;
	signal(SIGPIPE, SIG_IGN);
	return listenOnPort(port);
	return 0;
}


int hostname_to_ip(char *hostname, char *ip)
{
    int sockfd;  
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in *h;
    int rv;
 	printf("getting host info for %s\n", hostname);
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;
 
    if ( (rv = getaddrinfo( hostname , NULL , &hints , &servinfo)) != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
 
    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        h = (struct sockaddr_in *) p->ai_addr;
        strcpy(ip , inet_ntoa( h->sin_addr ) );
    }
     
    freeaddrinfo(servinfo); // all done with this structure
    return 0;
}


int isInCache(char *url){
	struct cache_object *current = cache;
	while(current!=NULL){
		if(strcmp(url, current->url)==0)
			return 1;
		current = current->next;
	}
	return 0;
}

void deliverResponse(char *message, int b_sock){
	if (send(b_sock, message, MAX_HTTP_RESPONSE, 0) < 0) {
		perror("Send error:");
		return 1;
	}

	printf("response Delivered\n");
	
	getData(b_sock);
}



void fetchContent(char *ip, char* message, int b_sock){
	int s_sock;
	struct sockaddr_in server_addr;
	char reply[MAX_HTTP_RESPONSE];
	if(d==1)printf("fetching content from ip %s with message %s\n", ip, message);
	if ((s_sock = socket(AF_INET, SOCK_STREAM/* use tcp */, 0)) < 0) {
		perror("Create socket error:");
		return;
	}

	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(80);


	if (connect(s_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		perror("Connect error:");
		return;
	}
	printf("connected\n");

	if (send(s_sock, message, MAX_HTTP_REQUEST, 0) < 0) {
			perror("Send error:");
			return;
		}
		int recv_len = read(s_sock, reply, MAX_HTTP_RESPONSE);
		if (recv_len < 0) {
			perror("Recv error:");
			return;
		}
		if(recv_len==0){
			close(s_sock);
			fetchContent(ip, message, b_sock);
			return;
		}
		// reply[recv_len] = 0;
		printf("Server reply %d:\n%s\n", recv_len, reply);
		deliverResponse(reply, b_sock);
		memset(reply, 0, sizeof(reply));

}

void parseResponse(char *message){

}

void parseRequest(char *message, int b_sock){
	char *copy = strdup(message);
	//Move past get and a space
	copy+=4;
	copy = strstr(copy,"Host: ");
	copy+=6;
	int i;
	for(i=0; i<strlen(copy); i++){
		if(copy[i]=='\n'){
			printf("foud newline! at %d first letter is %c\n", i, copy[0]);
			break;
		}
	}

	char *url = (char*)malloc(i * sizeof(char));
	strncpy(url, copy, i-1);
	url[i-1]='\0';
	printf("now is %s\n", url);

	//info is already in the cache
	if(isInCache(url)==1){
		if(d==1)printf("found in cache!\n");
	}
	//info is not in cache. Get its IP and then fetch it.
	else{
		char ip[IP_LENGTH];
		hostname_to_ip(url, ip);
		fetchContent(ip, message, b_sock);
	}


}

void parseMessage(char *message, int b_sock){
	// struct cache_object *cob = (struct cache_object*)malloc(sizeof(struct cache_object));
	// HTTP Request
	if(*message == 'G'){
		if(d==1)printf("parsing request\n");
		parseRequest(message, b_sock);
		return;
	}
	//HTTP Response
	if(*message == 'H'){
		if(d==1)printf("parsing response\n");

		parseResponse(message);
		return;
	}

	printf("neither request nor response\n");
	close(b_sock);
}

void getData(int b_sock){
	printf("reading message.....\n");

	char *message = (char*)malloc(MAX_HTTP_RESPONSE * sizeof(char));
	int recv_len = 0;
	if ((recv_len = recv(b_sock, message, MAX_HTTP_RESPONSE, 0)) < 0) {
		printf("rec error\n");
		perror("Recv error:");
		return 1;
	}
	printf("reading message.....: %s len: %d\n", message, recv_len);
	parseMessage(message, b_sock);
}
void *handle_connection(void *b_socket){
	
	int b_sock = (int)b_socket;
	getData(b_sock);
	

}

int listenOnPort(int port){
	int sock; int new_sock;
	int len;

	if ((sock = socket(AF_INET, SOCK_STREAM/* use tcp */, 0)) < 0) {
		perror("Create socket error:");
		return 1;
	}
	if(d==1)printf("socket created successfully!\n");

	struct sockaddr_in sin;

	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);

	if(bind(sock, (struct sockaddr*)&sin, sizeof(sin))<0){
		perror("Bind socket error");
		return 1;
	}

	if(d==1)printf("Socket Binding successfull\n");

	listen(sock,10);
	if(d==1)printf("Listen done, Waiting for connection \n");

	while(1){
		if ((new_sock = accept(sock, (struct sockaddr*)&sin, &len))<0){
			perror("Accept connection");
			return 1;
		}
		if(d==1)printf("accepted connection!\n");

		pthread_t conn_thread;
		pthread_create(&conn_thread, NULL, handle_connection, (void*)new_sock);
	}


	return 0;

}

