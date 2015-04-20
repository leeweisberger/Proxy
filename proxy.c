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
#include <sys/ioctl.h>
#include <fcntl.h> //fcntl


#define d 5
#define MAX_URL 2048
#define MAX_HTTP_HEADER 8000
#define MAX_HTTP_MESSAGE 500000
#define MAX_HTTP_RESPONSE (MAX_HTTP_MESSAGE + MAX_HTTP_HEADER)
#define MAX_HTTP_REQUEST MAX_HTTP_HEADER
#define IP_LENGTH 32
#define CHUNK_SIZE 1024


int cacheSize;
int maxCacheSize;

int getData(int b_sock, int s_sock);


struct cache_object{
	struct cache_object *next;
	char *url;
	char * data;
	int dataSize;
};
struct cache_object *cache;

int main(int argc, char ** argv){
	if(argc < 3){
		printf("Command should be: myprog <port> <sizeOfCach> (in MBs)\n");
		return 1;
	}


	int port = atoi(argv[1]);
	cacheSize = atoi(argv[2]);
	maxCacheSize = atoi(argv[2]);

	if(d==1)printf("port: %d\n", port);

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
 	if(d==1)printf("getting host info for %s\n", hostname);
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

// char * removeHeader(char * info){
// 	if(d==4)printf("remove Header info is %s\n", info);
// // 
// 	// char *copy = strdup(info);
// 	// if(d==4)printf("remove Header info is %s\n", info);
// 	// copy = strstr(copy, "Content-Length: ");
// 	// copy+=16;

// 	// int i;
// 	// for(i=0; i<strlen(copy); i++){
// 	// 	if(copy[i]=='\n'){
// 	// 		break;
// 	// 	}
// 	// }
// 	// char *size = (char*)malloc(i * sizeof(char));
// 	// strncpy(size, copy, i-1);
// 	// size[i-1]='\0';
// 	// int length = atoi(size);
// 	// if(d==4)printf("length of message is %d\n", length);

// 	// if(d==2)printf("Content length is %d\n", length);




// 	char *httpbody = strstr(info, "\r\n\r\n");
// 	httpbody+=4;

// 	char *httpbody2 = (char*)malloc(strlen(httpbody) * sizeof(char));
// 	strcpy(httpbody2, httpbody);
// 	return httpbody2;


// }

int getSize(struct cache_object *cob){
	int size = 0;
	size+=sizeof(cob->next);
	size+=strlen(cob->url);
	size+=strlen(cob->data);
	size+=sizeof(cob->dataSize);
	return size;
}
void removeLRU(){
	if(d==5)printf("removing LRU\n");
	if(cache->next==NULL){
		cacheSize+=getSize(cache);
		cache=NULL;
		return;
	}

	cacheSize+=getSize(cache);
	cache = cache->next;
	if(d==5)printf("new cache has %d space\n", cacheSize);


}

void createNewObject(struct cache_object *newObject, char * url, char * data, int size, struct cache_object *next){
	newObject->data = (char*)malloc(size * sizeof(char));
	memcpy(newObject->data, data, size);
	newObject->next=next;
	newObject->url = (char*)malloc(sizeof(char) * MAX_URL);
	strcpy(newObject->url, url);
	newObject->dataSize = size;
}

void addToCache(char *info, int size, char *url){
	//add ot the end of cache. Then check if we have to remove
	if(d==5)printf("to add to cache: %s\n", url);
	// char *trimmedInfo = removeHeader(info);
	struct cache_object *cob = (struct cache_object*)malloc(sizeof(struct cache_object));
	createNewObject(cob, url, info, size, NULL);
	if(getSize(cob) > maxCacheSize){
		printf("this message can never be stored in the cache b/c it's too big!!!\n");
		free(cob->data);
		free(cob->url);
		free(cob);
		return;
	}

	cacheSize-=getSize(cob);
	if(d==5)printf("cache size is now %d\n", cacheSize);

	while(cacheSize<0){
		if(d==5)printf("gotta make %d more room for this big boy\n", cacheSize*-1);
		removeLRU();
	}
	if(cache==NULL){
		cache=cob;
		return;
	}
	struct cache_object *current = cache;
	while(current->next!=NULL){
		current=current->next;
	}
	current->next = cob;
}


int isInCache(char *url){
	struct cache_object *current = cache;
	while(current!=NULL){
		if(strcmp(url, current->url)==0){
			if(d==5)printf("found in cache! %s\n", url);
			return 1;
		}
		current = current->next;
	}
	return 0;
}

void deliverResponse2(struct cache_object *object, int b_sock){
	if (write(b_sock, object->data, object->dataSize) < 0) {
		perror("Send error:");
		// deliverResponse(message, b_sock, s_sock);
		return;
	}

	if(d==5)printf("response Delivered\n");
	getData(b_sock, 0);
}
void deliverResponse(char *message, int size, int b_sock, int s_sock){
	if (write(b_sock, message, size) < 0) {
		perror("Send error:");
		// deliverResponse(message, b_sock, s_sock);
		return;
	}

	if(d==5)printf("response Delivered\n");
	getData(b_sock, s_sock);
}

char * getURL(char *request){
	char * request_copy = strdup(request);
	request_copy+=4;

	int i;
	for(i=0; i<strlen(request_copy); i++){
		if(request_copy[i]==' '){
			break;
		}
	}

	char *url = (char*)malloc(i * sizeof(char));
	strncpy(url, request_copy, i-1);
	url[i-1]='\0';
	return url;

}

int recv_timeout(int s , int timeout, char *result)
{
    int size_recv , total_size= 0;
    struct timeval begin , now;
    char chunk[CHUNK_SIZE];
    double timediff;
     
    //make socket non blocking
    fcntl(s, F_SETFL, O_NONBLOCK);
     
    //beginning time
    gettimeofday(&begin , NULL);
     
    while(1)
    {
        gettimeofday(&now , NULL);
         
        //time elapsed in seconds
        timediff = (now.tv_sec - begin.tv_sec) + 1e-6 * (now.tv_usec - begin.tv_usec);
         
        //if you got some data, then break after timeout
        if( total_size > 0 && timediff > timeout )
        {
            break;
        }
         
        //if you got no data at all, wait a little longer, twice the timeout
        else if( timediff > timeout*2)
        {
            break;
        }
         
        memset(chunk ,0 , CHUNK_SIZE);  //clear the variable
        if((size_recv =  recv(s , chunk , CHUNK_SIZE , 0) ) <= 0)
        {
            //if nothing was received then we want to wait a little before trying again, 0.1 seconds
            usleep(100000);
        }
        else
        {
        	// printf("got a chunk of size %d\n", size_recv);
        	memcpy(result+total_size, chunk, size_recv);

            total_size += size_recv;
            //reset beginning time
            gettimeofday(&begin , NULL);
        }
    }
    
    return total_size;
}

void fetchContent(char *ip, char* message, int req_size, int b_sock){
	int s_sock;
	struct sockaddr_in server_addr;


	if(d==4)printf("fetching content from ip %s with message %s\n", ip, message);
	if ((s_sock = socket(AF_INET, SOCK_STREAM/* use tcp */, 0)) < 0) {
		perror("Create socket error:");
		return;
	}

	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(80);


	if (connect(s_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		perror("Connect error:");
		close(s_sock);
		return;
	}
	if(d==1)printf("connected\n");

	if (send(s_sock, message, req_size, 0) < 0) {
			perror("Send error:");
			close(s_sock);
			return;
	}

	int recv_len=0;
	char *reply = (char*)malloc(MAX_HTTP_RESPONSE * sizeof(char));

	recv_len = recv_timeout(s_sock, 5, reply);
	printf ("timout recv len is %d\n", recv_len);
		
		
	addToCache(reply, recv_len, getURL(message));

	deliverResponse(reply, recv_len, b_sock, s_sock);
	memset(reply, 0, sizeof(reply));
}

char * getHostFromRequest(char *request){
	char *copy = strdup(request);
	//Move past get and a space
	copy+=4;
	copy = strstr(copy,"Host: ");
	copy+=6;
	int i;
	for(i=0; i<strlen(copy); i++){
		if(copy[i]=='\n'){
			break;
		}
	}

	char *url = (char*)malloc(i * sizeof(char));
	strncpy(url, copy, i-1);
	url[i-1]='\0';
	return url;
}

struct cache_object* moveToEndOfCache(char *url){
	struct cache_object *target=NULL;

	struct cache_object *current = cache;
	if(d==5)printf("before while loop\n");
	while(current->next!=NULL){
		if(d==5)printf("in while once! url: %s\n", current->next->url);

		if(strcmp(current->next->url, url)==0){
			if(d==5)printf("boom found in cache set to target\n");
			target = (struct cache_object*)malloc(sizeof(struct cache_object));
			struct cache_object *old = current->next;
			createNewObject(target, current->next->url, current->next->data, current->next->dataSize, NULL);
			current->next = current->next->next;
			// free(old);
		}
		if(current->next==NULL)break;
		current = current -> next;

	}
			if(d==5)printf("after while\n");

	current->next = target;
				if(d==5)printf("after while2\n");

	if(target==NULL){
		if(d==5)printf("found in cache as first item! url is: %s\n", cache->url);
		return cache;
	}
	if(d==5)printf("found in cache! url is: %s\n", target->url);

	return target;
}

// char * addHeaders(char *message){
// 	int index=0;
// 	char *response = (char*)malloc((MAX_HTTP_HEADER + strlen(message)) * sizeof(char));
// 	strcpy(response+index, "HTTP/1.1 200 OK\n");
// 	index+=strlen("HTTP/1.1 200 OK\n");
// 	strcpy(response+index, "Accept-Ranges: bytes\n");
// 	index+=strlen("Accept-Ranges: bytes\n");
// 	strcpy(response+index, "Vary: Accept-Encoding\n");
// 	index+=strlen("Vary: Accept-Encoding\n");
// 	strcpy(response+index, "Content-Encoding: gzip\n");
// 	index+=strlen("Content-Encoding: gzip\n");

// 	int contentLength = strlen(message);
// 	char *contentMessage;
// 	strcpy(contentMessage, "Content-Length: ");
// 	memcpy(contentMessage + strlen("Content-Length: "), strlen(message), strlen(strlen(message)));
// 	strcpy(contentMessage + strlen("Content-Length: ") + strlen(strlen(message)), "\n");
// //unsure of whether this will work
// 	strcpy(response+index, contentMessage);
// 	index+=strlen(contentMessage);

// 	strcpy(response+index, "Connection: keep-alive\n");

// 	if(d==4)printf("message with headers: %s\n", message);




// }

void parseRequest(char *message, int req_size, int b_sock){
	
	char *url = getHostFromRequest(message);
	char ip[IP_LENGTH];
	hostname_to_ip(url, ip);
	char *targetURL = getURL(message);
	//info is already in the cache
	if(isInCache(targetURL)==1){
		if(d==5)printf("found in cache!\n");
		struct cache_object *targetObject = moveToEndOfCache(targetURL);
		// char *targetMessageWithHeaders = addHeaders(targetMessage);
		// deliverResponse(targetMessage, strlen(targetMessage), b_sock, 0);
		deliverResponse2(targetObject, b_sock);



	}
	//info is not in cache. Get its IP and then fetch it.
	else{
		fetchContent(ip, message, req_size, b_sock);
	}


}

void parseMessage(char *message, int req_size, int b_sock){
	// struct cache_object *cob = (struct cache_object*)malloc(sizeof(struct cache_object));
	// HTTP Request

	if(*message == 'G'){
		if(d==5)printf("parsing request\n");
		parseRequest(message, req_size, b_sock);
		return;
	}

	if(d==1)printf("neither request nor response\n");
	close(b_sock);
}

int getData(int b_sock, int s_sock){
	if(d==1)printf("reading message.....\n");

		
	// printf ("len is : %d\n", len);
	

	char *message = (char*)malloc(MAX_HTTP_REQUEST * sizeof(char));
	int recv_len = 0;
	if ((recv_len = read(b_sock, message, MAX_HTTP_REQUEST)) <= 0) {
		// if(recv_len==0){
		// 	printf("getting data again causes request had size of 0\n");
		// 	getData(b_sock, s_sock);
		// 	return;
		// }
		printf("rec error closing both sockets\n");
		if(s_sock!=0)close(s_sock);
		if(recv_len==0)printf("holllllaaaaaa\n");

		perror("Recv error:");
		close(b_sock);
		return 0;
	}
	if(d==1)printf("reading message.....: %s len: %d\n", message, recv_len);
	parseMessage(message, recv_len, b_sock);
	return 1;
}
void *handle_connection(void *b_socket){
	
	int b_sock = (intptr_t)b_socket;
	
	// while( 1 ){
	int isData = getData(b_sock, 0);
		// if(isData==0)
			// break;
	// }
	// close(b_sock);
	

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
			continue;
		}
		if(d==5)printf("accepted connection!\n");

		pthread_t conn_thread;
		pthread_create(&conn_thread, NULL, handle_connection, (void*)(intptr_t)new_sock);
	}
	return 0;
}

