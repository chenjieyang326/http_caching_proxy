#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include <ctime>
#include <fstream>
#include <map>
#include <unordered_map>
#include <vector>

#include "utils.hpp"
#include "response.hpp"


class Proxy {
    private:
        const char * port;
    public:
        static unordered_map<string, Response_parser> cache;
        int proxy_fd;
    
    Proxy(const char * port) : port(port) {}
    void run();
    static void *handle(void * input);
    static void CONNECT_request(int client_fd, int client_id, int server_fd);
    static void POST_request(int client_fd, int client_id, int server_fd,
                  int content_len,
                  const Parser_request &parser_request);
    static void GET_request(int client_fd, int client_id, int server_fd, Parser_request & request_parsed);
    static void get_from_server(int client_fd, int client_id, int server_fd, Parser_request & request_parsed);
    static void add_to_cache(Response_parser & response_parsed, Parser_request & request_parsed, int no_store, int client_id);
};