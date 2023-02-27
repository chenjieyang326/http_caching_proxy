#include "utils.hpp"
#include <sys/socket.h>

using namespace std;

// Code derived from Prof. Rogers TCP example
int server_setup(const char *port) {
  int status;
  int socket_fd;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  const char *hostname = NULL;

  memset(&host_info, 0, sizeof(host_info));

  host_info.ai_family   = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;
  host_info.ai_flags    = AI_PASSIVE;

  status = getaddrinfo(hostname, port, &host_info, &host_info_list);
  if (status != 0) {
    cerr << "Error: cannot get address info for host" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } //if

  socket_fd = socket(host_info_list->ai_family, 
		     host_info_list->ai_socktype, 
		     host_info_list->ai_protocol);
  if (socket_fd == -1) {
    cerr << "Error: cannot create socket" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } //if

  int yes = 1;
  status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  status = bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1) {
    cerr << "Error: cannot bind socket" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } //if

  status = listen(socket_fd, 100);
  if (status == -1) {
    cerr << "Error: cannot listen on socket" << endl; 
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } //if
  freeaddrinfo(host_info_list);
  return socket_fd;
}

// Code derived from Prof. Rogers TCP example
int client_setup(const char * hostname, const char * port) {
  int status;
  int socket_fd;
  struct addrinfo host_info;
  struct addrinfo *host_info_list;

  memset(&host_info, 0, sizeof(host_info));
  host_info.ai_family   = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;

  status = getaddrinfo(hostname, port, &host_info, &host_info_list);
  if (status != 0) {
    cerr << "Error: cannot get address info for host" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } //if

  socket_fd = socket(host_info_list->ai_family, 
		     host_info_list->ai_socktype, 
		     host_info_list->ai_protocol);
  if (socket_fd == -1) {
    cerr << "Error: cannot create socket" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } //if

  status = connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  if (status == -1) {
    cerr << "Error: cannot connect to socket" << endl;
    cerr << "  (" << hostname << "," << port << ")" << endl;
    return -1;
  } //if

  freeaddrinfo(host_info_list);

  return socket_fd;
}

// Code derived from Prof. Rogers TCP example, updated by the group
int server_accept(int socket_fd, string & ip) {
  struct sockaddr_storage socket_addr;
  socklen_t socket_addr_len = sizeof(socket_addr);
  int client_connection_fd;
  client_connection_fd = accept(socket_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
  if (client_connection_fd == -1) {
    cerr << "Error: cannot accept connection on socket" << endl;
    return -1;
  } //if
  struct sockaddr_in * addr = (struct sockaddr_in *)&socket_addr;
  ip = inet_ntoa(addr->sin_addr);
  return client_connection_fd;
}

string receive_complete_message(int sender_fd, string & sender_message, int content_len) {
  int cum_len = 0, received_len = 0;

  for (;;) {
    if (cum_len >= content_len) break;
    char buffer[100000] = {0};
    if ((received_len = recv(sender_fd, &buffer, sizeof(buffer), MSG_NOSIGNAL)) <= 0) {
      break;
    }
    string buffer_string(buffer, received_len);
    sender_message += buffer_string;
    cum_len += received_len;
  }
  return sender_message;
}

int get_remaining_len(Response_parser & parsed_response, int response_len) {
  if (parsed_response.content_len != -1) {
    int head_end = parsed_response.response_content.find("\r\n\r\n");
    int body_len = response_len - head_end - 8;
    return parsed_response.content_len - body_len - 4;
  }
  else return -1;
}

int get_remaining_length(Parser_request & parsed_request, int request_len) {
  if (parsed_request.headers["Content-Length"] != "-1") {
    int head_end = parsed_request.request_content.find("\r\n\r\n");
    int body_len = request_len - head_end - 8;
    return stoi(parsed_request.headers["Content-Length"]) - body_len - 4;
  }
  else return -1;
}

