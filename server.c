#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>

#define ERR_EXIT(a) { perror(a); exit(1); }

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    
    int query_ID;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
    FILE* fp;
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list
int rc;

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";
const char* modifiable = "This account is modifiable.\n";
const char* operation_fail = "Operation failed.\n";
const char* account_locked = "This account is locked.\n";

// Forwards

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

static int handle_read(request* reqP);
// return 0: socket ended, request done.
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0.
// It's guaranteed that the header would be correctly set after the first read.
// error code:
// -1: client connection error


typedef struct
{
    int id;
    int balance;
}Account;

int main(int argc, char** argv) {
    int i, ret, query;
    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;
    int request_fail;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[512];
    int buf_len;

    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Get file descripter table size and initize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    fd_set master_fds, save_fds;
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 500000;
    FD_ZERO(&master_fds);
    FD_ZERO(&save_fds);
    FD_SET(svr.listen_fd, &save_fds);

    while (1)
    {
        memcpy(&master_fds, &save_fds, sizeof(save_fds));
        rc = select(maxfd + 1, &master_fds, NULL, NULL, &tv);
        int des_ready = rc;
        for(i = 3; i < maxfd && des_ready > 0; i++)
        {
            if(FD_ISSET(i, &master_fds))
            {
                if(i == svr.listen_fd)
                {
                    des_ready--;
                    clilen = sizeof(cliaddr);
                    conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);	
                    if (conn_fd < 0) {
                            if (errno == EINTR || errno == EAGAIN) continue;  // try again
                            if (errno == ENFILE) {
                                (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                                continue;
                            }
                            ERR_EXIT("accept")
                        }
            
                    FD_SET(conn_fd, &save_fds);
                    if(conn_fd > maxfd) maxfd = conn_fd;
                    requestP[conn_fd].conn_fd = conn_fd;
                    strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
                    fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);				
                }
                else
                {
                    des_ready--;
                    ret = handle_read(&requestP[i]); // parse data from client to requestP[conn_fd].buf
                    if (ret <= 0) {
                        fprintf(stderr, "bad request from %s\n", requestP[i].host);
                        break;
                    }
                    if(!requestP[i].wait_for_write) sscanf(requestP[i].buf, "%d", &query);
                    else query = requestP[i].query_ID;
#ifdef READ_SERVER
                    char buf[555];
                    FILE *fp;
                    Account client;
                    struct flock fl = {F_RDLCK, SEEK_SET, sizeof(Account)*(query-1), sizeof(Account), 0};
                    fl.l_pid = getpid();
                    fp = fopen("copy", "rb");
                    int fd = fileno(fp);
                    int k = fcntl(fd, F_SETLK, &fl);
                    printf("k = %d\n", k);
                    if(k == -1) write(requestP[i].conn_fd, account_locked, strlen(account_locked));
                    else
                    {
                        fseek(fp, sizeof(Account)*(query-1), SEEK_SET);
                        fread(&client, sizeof(Account), 1, fp);
                        sprintf(buf, "%s : %d %d\n", accept_read_header, client.id, client.balance);
                        write(requestP[i].conn_fd, buf, strlen(buf));
                        fl.l_type = F_UNLCK;
                        fcntl(fd, F_SETLK, &fl);
                        fclose(fp);
                    }
                        
#else
                    struct flock fl = { F_WRLCK, SEEK_SET, sizeof(Account)*(query-1), sizeof(Account), 0};
                    if(!(requestP[i].wait_for_write))
                    {
                        requestP[i].query_ID = query;
                        printf("query id = %d\n", query);
                        printf("check first\n");
                        fl.l_pid = getpid();
                        requestP[i].fp = fopen("copy","rb+");
                        int k = fcntl(fileno(requestP[i].fp), F_SETLK, &fl);
                        printf("k = %d\n", k);
                        if(errno == EACCES || errno == EAGAIN) printf("wrong\n");
                        if(k == -1) write(requestP[i].conn_fd, account_locked, strlen(account_locked));
                        else
                        {
                            write(requestP[i].conn_fd, modifiable, strlen(modifiable));
                            requestP[i].wait_for_write = 1;
                            continue;
                        }    
                    }
                    else
                    {
                        char action[25];
                        int num, num2, action_fail = 0;
                        if((requestP[i].buf)[0] != 't') sscanf(requestP[i].buf, "%s %d", action, &num);
                        else sscanf(requestP[i].buf, "%s %d %d", action, &num, &num2);
                        Account client, client2;
                        fseek(requestP[i].fp, sizeof(Account)*(query-1), SEEK_SET);
                        fread(&client, sizeof(Account), 1, requestP[i].fp);
                        printf("query id = %d\n", query);
                        printf("client id = %d balance = %d\n", client.id, client.balance);
                        if(action[0] == 's')
                        {
                            if(num < 0) action_fail = 1;
                            else
                            {
                                client.balance += num;
                                fseek(requestP[i].fp, sizeof(Account)*(query-1), SEEK_SET);
                                fwrite(&client, sizeof(Account), 1, requestP[i].fp);
                            }
                        }
                        else if(action[0] == 'w')
                        {
                            if(num < 0 || client.balance < num) action_fail = 1;
                            else
                            {
                                client.balance -= num;
                                fseek(requestP[i].fp, sizeof(Account)*(query-1), SEEK_SET);
                                fwrite(&client, sizeof(Account), 1, requestP[i].fp);
                            }
                        }
                        else if(action[0] == 't')
                        {
                            fseek(requestP[i].fp, sizeof(Account)*(num-1), SEEK_SET);
                            fread(&client2, sizeof(Account), 1, requestP[i].fp);   //don't need to check if client2 is locked
                            if(num2 < 0 || client.balance < num2) action_fail = 1;
                            else
                            {
                                client.balance -= num2;
                                client2.balance += num2;
                                fseek(requestP[i].fp, sizeof(Account)*(query-1), SEEK_SET);
                                fwrite(&client, sizeof(Account), 1, requestP[i].fp);
                                fseek(requestP[i].fp, sizeof(Account)*(num-1), SEEK_SET);
                                fwrite(&client2, sizeof(Account), 1, requestP[i].fp);
                            }
                        }
                        else
                        {
                            if(num <= 0) action_fail = 1;
                            else
                            {
                                client.balance = num;
                                fseek(requestP[i].fp, sizeof(Account)*(query-1), SEEK_SET);
                                fwrite(&client, sizeof(Account), 1, requestP[i].fp);
                            }
                        }
                        fl.l_type = F_UNLCK;
                        fl.l_whence = SEEK_SET;
                        fl.l_start = sizeof(Account)*(query-1);
                        fl.l_len = sizeof(Account);
                        fl.l_pid = getpid();
                        fcntl(fileno(requestP[i].fp), F_SETLK, &fl);
                        fclose(requestP[i].fp);
                        if(action_fail) write(requestP[i].conn_fd, operation_fail, strlen(operation_fail));
			else printf("id = %d balance = %d\n", client.id, client.balance);
                    }
                                            
                    sprintf(buf,"%s : %s\n",accept_write_header, requestP[i].buf);
                    write(requestP[i].conn_fd, buf, strlen(buf));
                    requestP[i].wait_for_write = 0;
#endif
                    close(requestP[i].conn_fd);
                    free_request(&requestP[i]);
                    FD_CLR(i, &save_fds);	
                }
            }
        }
    }
    free(requestP);
    return 0;
}


// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void* e_malloc(size_t size);

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->query_ID = 0;
    reqP->wait_for_write = 0;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

// return 0: socket ended, request done.
// return 1: success, message (without header) got this time is in reqP->buf with reqP->buf_len bytes. read more until got <= 0.
// It's guaranteed that the header would be correctly set after the first read.
// error code:
// -1: client connection error
static int handle_read(request* reqP) {
    int r;
    char buf[555];
    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
	char* p1 = strstr(buf, "\015\012");
	int newline_len = 2;
	// be careful that in Windows, line ends with \015\012
	if (p1 == NULL) {
		p1 = strstr(buf, "\012");
		newline_len = 1;
		if (p1 == NULL) {
			ERR_EXIT("this really should not happen...");
		}
	}
	size_t len = p1 - buf + 1;
	memmove(reqP->buf, buf, len);
	reqP->buf[len - 1] = '\0';
	reqP->buf_len = len-1;
    return 1;
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }
}

static void* e_malloc(size_t size) {
    void* ptr;

    ptr = malloc(size);
    if (ptr == NULL) ERR_EXIT("out of memory");
    return ptr;
}
