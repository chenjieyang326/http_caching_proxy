#include <cstring>
#include <iostream>
#include <string>

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <vector>

#include "parser_response.h"
#include "parser_request.h"
using namespace std;

int server_setup(const char *port);
int client_setup(const char * hostname, const char * port);
int server_accept(int socket_fd, string & ip);
string receive_complete_message(int sender_fd,  string & sender_message, int content_len);
int get_remaining_length(Response_parser & parsed_response, int response_len);
int get_remaining_length(Parser_request & parsed_request, int request_len);