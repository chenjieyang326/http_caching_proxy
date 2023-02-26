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
        unordered_map<string, Response> cache;
        int proxy_fd;
    
    Proxy(const char * port) : port(port), cache(50) {}
    void run();
    static void *handle(void * input);
    static void CONNECT_request(int client_fd, int client_id, int server_fd);
};