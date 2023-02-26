#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <sys/select.h>
#include <sys/socket.h>
#include <unordered_map>
#include <vector>
#include <cstring>

#include "client.hpp"
#include "parser_request.hpp"
#include "proxy.hpp"
#include "utils.hpp"

using namespace std;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
ofstream logFile("/var/log/erss/proxy.log");

string getCurrTime() {
  time_t now = std::time(nullptr);
  stringstream ss;
  ss << put_time(std::gmtime(&now), "%a, %d %b %Y %H:%M:%S GMT");
  return ss.str();
}

void Proxy::run() {
  proxy_fd = server_setup(port);
  if (proxy_fd == -1) {
    cout << "Unable to create proxy socket" << endl;
    return;
  }
  int client_id = 0;
  for (;;) {
    int client_fd;
    string client_ip;
    client_fd = server_accept(proxy_fd, client_ip);
    if (client_fd == -1) {
      cout << "Errorr in connecting client" << endl;
      continue;
    }
    pthread_t thread;
    pthread_mutex_lock(&mutex);
    Client *client = new Client(client_fd, client_id, client_ip);
    client_id++;
    pthread_mutex_unlock(&mutex);
    pthread_create(&thread, NULL, handle, client);
  }
}

void *Proxy::handle(void *input) {
  Client *client = (Client *)input;
  int client_fd = client->client_fd;
  int client_id = client->client_id;
  string client_ip = client->client_ip;
  char request_message[100000] = {0};
  int len = recv(client_fd, &request_message, sizeof(request_message), 0);
  if (len <= 0) {
    pthread_mutex_lock(&mutex);
    cout << client_id << "Invalid Request" << endl;
    pthread_mutex_unlock(&mutex);
    return NULL;
  }
  string request_content;
  request_content.assign(request_message, len);
  Parser_request request_parsed(request_content);
  pthread_mutex_lock(&mutex);
  logFile << client_id << ": \"" << request_parsed.first_line << "\" from " << client_ip
          << " @ " << getCurrTime().append("\0");
  pthread_mutex_unlock(&mutex);
  if (request_parsed.method != "GET" && request_parsed.method != "POST" &&
      request_parsed.method != "CONNECT") {
    const char _400_Bad_Request[100] = "HTTP/1.1 400 Bad Request";
    send(client_fd, &_400_Bad_Request, sizeof(_400_Bad_Request), MSG_NOSIGNAL);
    pthread_mutex_lock(&mutex);
    logFile << client_id << ": Responding \"HTTP/1.1 400 Bad Request\""
            << std::endl;
    pthread_mutex_unlock(&mutex);
  } else {
    const char * request_hostname = request_parsed.hostname.c_str();
    const char * port = request_parsed.port.c_str();
    int server_fd = client_setup(request_hostname, port);
    if (request_parsed.method == "GET") {
      // handle GET request
    } else if (request_parsed.method == "POST") {
      // handle POST request
    } else {
      // handle CONNECT request
      CONNECT_request(client_fd, client_id, server_fd);
    }
  }
  return NULL;
}

void Proxy::CONNECT_request(int client_fd, int client_id, int server_fd) {
  // send 200 OK to client
  const char CONNECT_message[100] = "HTTP/1.1 200 OK\r\n\r\n";
  send(client_fd, &CONNECT_message, sizeof(CONNECT_message), MSG_NOSIGNAL);

  // write 200 OK to log
  pthread_mutex_lock(&mutex);
  logFile << client_id << ": Responding \"HTTP/1.1 200 OK\"" << endl;
  pthread_mutex_unlock(&mutex);

  // listen to response, send to other
  fd_set readfds;
  int nfds = client_fd > server_fd ? client_fd : server_fd;

  for (;;) {
    FD_ZERO(&readfds);
    FD_SET(client_fd, &readfds);
    FD_SET(server_fd, &readfds);
    select(nfds + 1, &readfds, NULL, NULL, NULL);

    int len_received_client, len_sent_client;

    char received_client_message[100000] = {0};
    if (FD_ISSET(client_fd, &readfds)) {
      len_received_client = recv(client_fd, &received_client_message, sizeof(received_client_message), MSG_NOSIGNAL);
      if (len_received_client <= 0) {
        pthread_mutex_lock(&mutex);
        cout << "Fail to receive client message from tunnel" << endl;
        logFile << client_id << ": Tunnel closed" << std::endl;
        pthread_mutex_unlock(&mutex);
        return;
      }
      len_sent_client = send(server_fd, &received_client_message, sizeof(received_client_message), MSG_NOSIGNAL);
      if (len_sent_client <= 0) {
        pthread_mutex_lock(&mutex);
        cout << "Fail to send client message from tunnel" << endl;
        logFile << client_id << ": Tunnel closed" << std::endl;
        pthread_mutex_unlock(&mutex);
        return;
      }
    }

    int len_received_server, len_sent_server;
    char received_server_message[100000] = {0};
    if (FD_ISSET(server_fd, &readfds)) {
      len_received_server = recv(server_fd, &received_server_message, sizeof(received_server_message), MSG_NOSIGNAL);
      if (len_received_server <= 0) {
        pthread_mutex_lock(&mutex);
        cout << "Fail to receive server message from tunnel" << endl;
        logFile << client_id << ": Tunnel closed" << std::endl;
        pthread_mutex_unlock(&mutex);
        return;
      }
      len_sent_server = send(client_fd, &received_server_message, sizeof(received_server_message), MSG_NOSIGNAL);
      if (len_sent_server <= 0) {
        pthread_mutex_lock(&mutex);
        cout << "Fail to send server message from tunnel" << endl;
        logFile << client_id << ": Tunnel closed" << std::endl;
        pthread_mutex_unlock(&mutex);
        return;
      }
    }
  }
}