#include <string>

class Client {
    public:
        int client_fd;
        int client_id;
        std::string client_ip;
    Client() {}
    Client(int client_fd, int client_id, std::string client_ip) : client_fd(client_fd), 
                                                                  client_id(client_id),
                                                                  client_ip(client_ip) {}
};