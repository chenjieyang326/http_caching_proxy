#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <pthread.h>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unordered_map>

#include "client.h"
#include "proxy.h"

using namespace std;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
ofstream logFile("proxy.log");
ofstream chunkout("chunkout.log");
ofstream request("request.log");
static unordered_map<string, Response_parser> cache;

string getCurrTime() {
  time_t now = std::time(nullptr);
  stringstream ss;
  ss << put_time(std::gmtime(&now), "%a %b %d %H:%M:%S %Y");
  return ss.str();
}

void Proxy::run() {
  proxy_fd = server_setup(port);
  if (proxy_fd == -1) {
    pthread_mutex_lock(&mutex);
    cout << "Unable to create proxy socket" << endl;
    pthread_mutex_unlock(&mutex);
    return;
  }
  int client_id = 0;
  int client_fd;
  for (;;) {
    string client_ip;
    client_fd = server_accept(proxy_fd, &client_ip);
    if (client_fd == -1) {
      pthread_mutex_lock(&mutex);
      cout << "Errorr in connecting client" << endl;
      pthread_mutex_unlock(&mutex);
      continue;
    }
    pthread_t thread;
    pthread_mutex_lock(&mutex);
    Client *client = new Client(client_fd, client_id, client_ip);
    // client_id++;
    cout << "client connected as client: " << client_id << endl;
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
  // cout << "client ip is: " << client_ip << endl;
  char request_message[65536] = {0};
  int len = recv(client_fd, &request_message, sizeof(request_message), 0);
  cout << "received request with len: " << len << endl;
  if (len <= 0) {
    pthread_mutex_lock(&mutex);
    cout << client_id << ": Invalid Request" << endl;
    pthread_mutex_unlock(&mutex);
    return NULL;
  }
  string request_message_str(request_message);
  if (request_message_str == "" || request_message_str == "\r" ||
      request_message_str == "\n" || request_message_str == "\r\n")
    return NULL;
  string request_content(request_message, len);
  Parser_request *request_parsed = new Parser_request(request_content);
  if (request_parsed->method != "GET" && request_parsed->method != "POST" &&
      request_parsed->method != "CONNECT") {
    const char _400_Bad_Request[100] = "HTTP/1.1 400 Bad Request";
    size_t _400_Bad_Request_len = string("HTTP/1.1 400 Bad Request").size() + 1;
    send(client_fd, &_400_Bad_Request, _400_Bad_Request_len, MSG_NOSIGNAL);
    pthread_mutex_lock(&mutex);
    logFile << client_id << ": Responding \"HTTP/1.1 400 Bad Request\""
            << std::endl;
    pthread_mutex_unlock(&mutex);
    return NULL;
  }
  pthread_mutex_lock(&mutex);
  logFile << client_id << ": \"" << request_parsed->first_line << "\" from "
          << client_ip << " @ " << getCurrTime().append("\0") << endl;
  pthread_mutex_unlock(&mutex);
  const char *request_hostname = request_parsed->hostname.c_str();
  const char *port = request_parsed->port.c_str();
  cout << "#################################" << endl;
  cout << request_hostname << endl;
  cout << port << endl;
  cout << "#################################" << endl;
  int server_fd = client_setup(request_hostname, port);
  if (server_fd == -1) {
    pthread_mutex_lock(&mutex);
    cout << "Failed in connecting server" << endl;
    pthread_mutex_unlock(&mutex);
    return NULL;
  }
  if (request_parsed->method == "GET") {
    // handle GET request
    GET_request(client_fd, client_id, server_fd, *request_parsed);
  } else if (request_parsed->method == "POST") {
    // handle POST request
    int content_len = get_remaining_length(*request_parsed, len);
    POST_request(client_fd, client_id, server_fd, content_len, *request_parsed);
  } else {
    // handle CONNECT request
    CONNECT_request(client_fd, client_id, server_fd);
  }
  close(server_fd);
  close(client_fd);
  return NULL;
}

void Proxy::CONNECT_request(int client_fd, int client_id, int server_fd) {
  // send 200 OK to client
  const char CONNECT_message[100] = "HTTP/1.1 200 OK\r\n\r\n";
  send(client_fd, &CONNECT_message, 19, MSG_NOSIGNAL);

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
    int select_flag = select(nfds + 1, &readfds, NULL, NULL, NULL);
    if (select_flag <0) {
      logFile << "Select Failed in CONNECT Request" << endl;
      return;
    }

    int len_received_client, len_sent_client;
    char received_client_message[100000] = {0};
    if (FD_ISSET(client_fd, &readfds)) {
      len_received_client = recv(client_fd, &received_client_message,
                                 sizeof(received_client_message), MSG_NOSIGNAL);
      if (len_received_client <= 0) {
        pthread_mutex_lock(&mutex);
        cout << "Fail to receive client message from tunnel" << endl;
        logFile << client_id << ": Tunnel closed" << std::endl;
        pthread_mutex_unlock(&mutex);
        return;
      }
      len_sent_client = send(server_fd, &received_client_message,
                             len_received_client, MSG_NOSIGNAL);
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
      len_received_server = recv(server_fd, &received_server_message,
                                 sizeof(received_server_message), MSG_NOSIGNAL);
      // check 502
      // string _502_checker_tmp(received_server_message);
      // Response_parser _502_checker(_502_checker_tmp);
      // if (check_502(_502_checker, client_fd, client_id)) {
      //   pthread_mutex_lock(&mutex);
      //   cout << "Responding 502" << endl;
      //   logFile << client_id << ": Tunnel closed" << std::endl;
      //   pthread_mutex_unlock(&mutex);
      //   return;
      // }

      if (len_received_server <= 0) {
        pthread_mutex_lock(&mutex);
        cout << "Fail to receive server message from tunnel" << endl;
        logFile << client_id << ": Tunnel closed" << std::endl;
        pthread_mutex_unlock(&mutex);
        return;
      }
      len_sent_server = send(client_fd, &received_server_message,
                             len_received_server, MSG_NOSIGNAL);
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

void Proxy::POST_request(int client_fd, int client_id, int server_fd,
                         int content_len,
                         const Parser_request &parser_request) {
  pthread_mutex_lock(&mutex);
  logFile << client_id << ": "
          << "Requesting \"" << parser_request.first_line << "\" from "
          << parser_request.url << endl;
  pthread_mutex_unlock(&mutex);
  if (content_len == -1) {
    pthread_mutex_lock(&mutex);
    cout << "Cannot get request content length in POST request" << endl;
    pthread_mutex_unlock(&mutex);
    return;
  } else {
    string request_content = parser_request.request_content;
    string complete_request =
        receive_complete_message(client_fd, request_content, content_len);
    char request_to_send[complete_request.length() + 1];
    strcpy(request_to_send, complete_request.c_str());
    // send complete request to server
    send(server_fd, &request_to_send, sizeof(request_to_send), MSG_NOSIGNAL);
    // receive server response
    char server_response_buffer[100000] = {0};
    int server_response_len =
        recv(server_fd, &server_response_buffer, sizeof(server_response_buffer),
             MSG_NOSIGNAL);
    if (server_response_len <= 0) {
      pthread_mutex_lock(&mutex);
      cout << "Cannot receive server response from POST request" << endl;
      pthread_mutex_unlock(&mutex);
      return;
    }
    string server_response_str = string(server_response_buffer);
    Response_parser parser_response(server_response_str);
    if (check_502(parser_response, client_fd, client_id))
      return;
    pthread_mutex_lock(&mutex);
    logFile << client_id << ": Received \"" << parser_response.firstLine
            << "\" from " << parser_request.url << endl;
    pthread_mutex_unlock(&mutex);

    send(client_fd, &server_response_buffer, server_response_len,
         MSG_NOSIGNAL);
    pthread_mutex_lock(&mutex);
    logFile << client_id << ": Responding \"" << parser_response.firstLine
            << "\"" << endl;
    pthread_mutex_unlock(&mutex);
  }
}

void Proxy::GET_request(int client_fd, int client_id, int server_fd,
                        Parser_request &request_parsed) {
  string request_url = request_parsed.url;
  unordered_map<string, Response_parser>::iterator it = cache.begin();
  it = cache.find(request_url);
  if (it == cache.end()) { // not found in cache
    pthread_mutex_lock(&mutex);
    logFile << client_id << ": not in cache" << endl;
    pthread_mutex_unlock(&mutex);
    pthread_mutex_lock(&mutex);
    logFile << client_id << ": Requesting \"" << request_parsed.first_line
            << "\" from " << request_parsed.url << endl;
    pthread_mutex_unlock(&mutex);
    string request_message_str = request_parsed.request_content;
    char request_message[100000];
    strcpy(request_message, request_message_str.c_str());
    request << request_message << endl;
    send(server_fd, &request_message, request_message_str.size() + 1, MSG_NOSIGNAL);
    get_from_server(client_fd, client_id, server_fd, request_parsed);
  } else { // found in cache
    int no_cache = (it->second.CacheControl.find("no-cache") != string::npos);
    if (no_cache) {
      if (!revalidate(it->second, server_fd, client_id, client_fd)) {
        // ask server
        pthread_mutex_lock(&mutex);
        logFile << client_id << ": Requesting \"" << it->second.firstLine
                << "\" from " << request_parsed.url << endl;
        pthread_mutex_unlock(&mutex);
        string original_request_message_str = request_parsed.request_content;
        char original_request_message[original_request_message_str.size() + 1];
        strcpy(original_request_message, original_request_message_str.c_str());
        send(server_fd, &original_request_message,
             original_request_message_str.size() + 1, MSG_NOSIGNAL);
        get_from_server(client_fd, client_id, server_fd, request_parsed);
      } else {
        // use cache
        char cached_response[it->second.response_content.size() + 1];
        strcpy(cached_response, it->second.response_content.c_str());
        send(client_fd, &cached_response, sizeof(cached_response),
             MSG_NOSIGNAL);
        pthread_mutex_lock(&mutex);
        logFile << client_id << ": Responding \"" << it->second.firstLine
                << "\"" << endl;
        pthread_mutex_unlock(&mutex);
      }
    } else {
      int valid = check_expire(server_fd, request_parsed, it->second, client_id,
                               client_fd); // = check_time
      if (valid) {
        // use cache
        char cached_response[it->second.response_content.size() + 1];
        strcpy(cached_response, it->second.response_content.c_str());
        send(client_fd, &cached_response, sizeof(cached_response),
             MSG_NOSIGNAL);
        pthread_mutex_lock(&mutex);
        logFile << client_id << ": Responding \"" << it->second.firstLine
                << "\"" << endl;
        pthread_mutex_unlock(&mutex);
      } else {
        // ask server
        pthread_mutex_lock(&mutex);
        logFile << client_id << ": Requesting \"" << it->second.firstLine
                << "\" from " << request_parsed.url << endl;
        pthread_mutex_unlock(&mutex);
        string original_request_message_str = request_parsed.request_content;
        char original_request_message[original_request_message_str.size() + 1];
        strcpy(original_request_message, original_request_message_str.c_str());
        send(server_fd, &original_request_message,
             sizeof(original_request_message), MSG_NOSIGNAL);
        get_from_server(client_fd, client_id, server_fd, request_parsed);
      }
    }
  }
}

void Proxy::get_from_server(int client_fd, int client_id, int server_fd,
                            Parser_request &request_parsed) {
  char server_response_buffer[100000] = {0};
  int server_response_buffer_len =
      recv(server_fd, &server_response_buffer, sizeof(server_response_buffer),
           MSG_NOSIGNAL);
  if (server_response_buffer_len <= 0) {
    cout << "Fail to receive response from server in GET request (not in cache)"
         << endl;
    return;
  }
  string server_response_buffer_str(server_response_buffer);
  Response_parser response_parsed(server_response_buffer_str);
  if (check_502(response_parsed, client_fd, client_id))
    return;
  pthread_mutex_lock(&mutex);
  logFile << client_id << ": Received \"" << response_parsed.firstLine
          << "\" from " << request_parsed.url << endl;

  pthread_mutex_unlock(&mutex);
  int is_chunk =
      (response_parsed.response_content.find("chunked") != string::npos);

  if (is_chunk) {
    // cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << endl;
    pthread_mutex_lock(&mutex);
    cout << client_id << ": not cacheable because it is chunked" << endl;
    pthread_mutex_unlock(&mutex);
    // cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
    // send first chunk to client
    send(client_fd, &server_response_buffer, server_response_buffer_len,
         MSG_NOSIGNAL);
    // chunkout << server_response_buffer << endl;
    // cout << server_response_buffer << endl;
    // cout << "first message being sent is: \n" << server_response_buffer <<
    // endl; send rest
    char remaining_chunk[28000] = {0};
    int chunk_id = 0;
    for (;;) {
      // break;
      int remaining_chunk_len = recv(server_fd, &remaining_chunk,
                                     sizeof(remaining_chunk), MSG_NOSIGNAL);
      // chunkout << remaining_chunk << endl;
      // string complete(server_response_buffer);
      // complete += string(remaining_chunk);
      // char complete_to_send[complete.size() + 1];
      // strcpy(complete_to_send, complete.c_str());
      // send(client_fd, &complete_to_send, sizeof(complete_to_send), MSG_NOSIGNAL);
      // cout << "-----------------------------" << endl;
      // cout << remaining_chunk << endl;
      // cout << "-----------------------------" << endl;
      // break;
      // // cout << chunk_id <<": get remaining chunk len: " << remaining_chunk_len
      // << endl; string remaining_chunk_str(remaining_chunk); size_t pos =
      // remaining_chunk_str.find("\r\n"); string status_str =
      // remaining_chunk_str.substr(0, pos); cout << chunk_id << ": size: " <<
      // remaining_chunk_len << endl; chunk_id++; int URI_TOO_LONG =
      // (string(remaining_chunk).find("URI Too Long") != string::npos); if
      // (remaining_chunk_len <= 0 || URI_TOO_LONG) {
      //   break;
      // }
      if (remaining_chunk_len <= 0)
        break;
      // if (status_str == "0") break;
      // if (chunk_id == 0) {
      //   send(client_fd, &remaining_chunk, remaining_chunk_len, MSG_NOSIGNAL);
      //   // cout << remaining_chunk;
      //   // chunkout << remaining_chunk;
      //   break;
      // }
      send(client_fd, &remaining_chunk, remaining_chunk_len, MSG_NOSIGNAL);
    }
    // cout << "out of loop for chunk messasge receive" << endl;
    // string complete_message(server_response_buffer);
    // for (;;) {
    //   char remaining_chunk[100000] = {0};
    //   int remaining_chunk_len = recv(server_fd, &remaining_chunk,
    //                                  sizeof(remaining_chunk), MSG_NOSIGNAL);
    //   if (remaining_chunk_len <= 0)
    //     break;
    //   string new_chunk(remaining_chunk);
    //   complete_message += new_chunk;
    // }
    // char complete_message_to_send[complete_message.size() + 1];
    // strcpy(complete_message_to_send, complete_message.c_str());
    // send(client_fd, &complete_message_to_send,
    // sizeof(complete_message_to_send),
    //      MSG_NOSIGNAL);
  } else {
    int no_store =
        (response_parsed.response_content.find("no-store") != string::npos);
    // can log cache control
    // log_cache_control();
    int content_len =
        get_remaining_length(response_parsed, server_response_buffer_len);
    if (content_len != -1) {
      string sender_message = response_parsed.response_content;
      string complete_response =
          receive_complete_message(server_fd, sender_message, content_len);
      response_parsed.response_content = complete_response;
      char response_to_send[complete_response.length() + 1];
      strcpy(response_to_send, complete_response.c_str());
      send(client_fd, &response_to_send, sizeof(response_to_send),
           MSG_NOSIGNAL);
    } else {
      char response_to_send[response_parsed.response_content.length() + 1];
      strcpy(response_to_send, response_parsed.response_content.c_str());
      send(client_fd, &response_to_send, sizeof(response_to_send),
           MSG_NOSIGNAL);
    }
    add_to_cache(response_parsed, request_parsed, no_store, client_id);
  }
  pthread_mutex_lock(&mutex);
  logFile << client_id << ": Responding \"" << response_parsed.firstLine << "\""
          << endl;
  pthread_mutex_unlock(&mutex);
}

void Proxy::add_to_cache(Response_parser &response_parsed,
                         Parser_request &request_parsed, int no_store,
                         int client_id) {
  int _200_OK = (response_parsed.response_content.find("HTTP/1.1 200 OK") !=
                 string::npos);
  if (_200_OK) {
    if (no_store) {
      pthread_mutex_lock(&mutex);
      logFile << client_id << ": not cacheable because no-store" << endl;
      pthread_mutex_unlock(&mutex);
      return;
    } else {
      if (response_parsed.maxAge != -1) {
        time_t expire_time =
            response_parsed.convertedDate + response_parsed.maxAge;
        struct tm *asc_time = gmtime(&expire_time);
        const char *t = asctime(asc_time);
        pthread_mutex_lock(&mutex);
        logFile << client_id << ": cached, expires at " << t;
        pthread_mutex_unlock(&mutex);
      } else if (response_parsed.convertedExpires != -1) {
        time_t expire_time = response_parsed.convertedExpires;
        struct tm *asc_time = gmtime(&expire_time);
        const char *t = asctime(asc_time);
        pthread_mutex_lock(&mutex);
        logFile << client_id << ": cached, expires at " << t;
        pthread_mutex_unlock(&mutex);
      }
    }
    if (cache.size() >= 20) {
      unordered_map<string, Response_parser>::iterator it = cache.begin();
      cache.erase(it);
    }
    cache.insert(
        pair<string, Response_parser>(request_parsed.url, response_parsed));
  }
}

int Proxy::revalidate(Response_parser &response_parsed, int server_fd,
                      int client_id, int client_fd) {
  int flag = (response_parsed.Etag == "" && response_parsed.LastModified == "");
  if (flag)
    return 1;

  string updated_response_str = response_parsed.response_content;

  if (response_parsed.Etag != "") {
    string addEtag = "If-None-Match: " + response_parsed.Etag.append("\r\n");
    updated_response_str =
        updated_response_str.insert(updated_response_str.length() - 2, addEtag);
  }
  if (response_parsed.LastModified != "") {
    string addLastModified =
        "If-Modified-Since: " + response_parsed.LastModified.append("\r\n");
    updated_response_str = updated_response_str.insert(
        updated_response_str.length() - 2, addLastModified);
  }
  char new_request[updated_response_str.length() + 1];
  strcpy(new_request, updated_response_str.c_str());
  if (send(server_fd, &new_request, sizeof(new_request), MSG_NOSIGNAL) > 0) {
    pthread_mutex_lock(&mutex);
    cout << "send revalidation success from GET request" << endl;
    pthread_mutex_unlock(&mutex);
  }
  char new_response_buffer[100000] = {0};
  int new_response_buffer_len = recv(server_fd, &new_response_buffer,
                                     sizeof(new_response_buffer), MSG_NOSIGNAL);
  // check 502
  string _502_checker_str(new_response_buffer);
  Response_parser _502_checker_new_response(_502_checker_str);
  if (check_502(_502_checker_new_response, client_fd, client_id))
    return 0;
  if (new_response_buffer_len <= 0) {
    pthread_mutex_lock(&mutex);
    cout << "receive revalidation failed from GET request" << endl;
    pthread_mutex_unlock(&mutex);
  }
  string new_response_str(new_response_buffer, new_response_buffer_len);
  int _200_OK = (new_response_str.find("HTTP/1.1 200 OK") != string::npos);
  if (_200_OK) {
    pthread_mutex_lock(&mutex);
    logFile << client_id << ": in cache, requires validation" << endl;
    pthread_mutex_unlock(&mutex);
    return 0;
  }
  return 1;
}

int Proxy::check_expire(int server_fd, Parser_request &request_parsed,
                        Response_parser &response_parsed, int client_id,
                        int client_fd) {
  time_t curr = time(0);
  if (response_parsed.maxAge != -1) {
    if (response_parsed.convertedDate + response_parsed.maxAge < curr) {
      cache.erase(request_parsed.url);
      time_t expire_time =
          response_parsed.convertedDate + response_parsed.maxAge;
      struct tm *asc_time = gmtime(&expire_time);
      const char *t = asctime(asc_time);
      pthread_mutex_lock(&mutex);
      logFile << client_id << ": in cache, but expired at " << t;
      pthread_mutex_unlock(&mutex);
      return 0;
    }
  }
  if (response_parsed.convertedExpires != -1) {
    if (curr > response_parsed.convertedExpires) {
      cache.erase(request_parsed.url);
      time_t expire_time = response_parsed.convertedExpires;
      struct tm *asc_time = gmtime(&expire_time);
      const char *t = asctime(asc_time);
      pthread_mutex_lock(&mutex);
      logFile << client_id << ": in cache, but expired at " << t;
      pthread_mutex_unlock(&mutex);
      return 0;
    }
  }
  int pass_revalid =
      revalidate(response_parsed, server_fd, client_id, client_fd);
  if (!pass_revalid) {
    return 0;
  }
  pthread_mutex_lock(&mutex);
  logFile << client_id << ": in cache, valid" << endl;
  pthread_mutex_unlock(&mutex);
  return 1;
}

int Proxy::check_502(Response_parser &response_parsed, int client_fd,
                     int client_id) {
  if (response_parsed.status == "502 Bad Gateway") {
    string _502_message_str = "HTTP/1.1 502 Bad Gateway";
    char _502_message[_502_message_str.size() + 1];
    strcpy(_502_message, _502_message_str.c_str());
    send(client_fd, &_502_message, sizeof(_502_message), MSG_NOSIGNAL);
    pthread_mutex_lock(&mutex);
    logFile << client_id << ": Responding " << _502_message_str << endl;
    pthread_mutex_unlock(&mutex);
    return 1;
  }
  return 0;
}