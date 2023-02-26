#include <cstring>
#include <iostream>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <string>

using namespace std;
int server_setup(const char *port);
int client_setup(const char * hostname, const char * port);
int server_accept(int socket_fd, string & ip);