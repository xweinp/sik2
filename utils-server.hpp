#include <poll.h>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> 
#include <fcntl.h> 

using namespace std;

int ipv6_enabled_sock(uint16_t port) {
    int socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        return -1; // Cannot create socket
    }

    int off = 0;
    int res = setsockopt(
        socket_fd, 
        IPPROTO_IPV6, 
        IPV6_V6ONLY, 
        &off,
        sizeof(off)
    );

    if (res < 0) {
        close(socket_fd);
        return -1; // Cannot set IPV6_V6ONLY option
    }

    sockaddr_in6 addr6{};
    addr6.sin6_family = AF_INET6;
    addr6.sin6_addr = in6addr_any;
    addr6.sin6_port = htons(port);

    res = bind(
        socket_fd,
        (sockaddr*)&addr6,
        sizeof(addr6)
    );

    if (res < 0) {
        close(socket_fd);
        return -1; // Cannot bind socket
    }

    res = listen(socket_fd, SOMAXCONN);

    if (res < 0) {
        close(socket_fd);
        return -1; // Cannot listen on socket
    }
    
    return socket_fd;
}

int ipv4_only_sock(uint16_t port) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0) {
        return -1; // Cannot create socket
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    int res = bind(
        socket_fd,
        (sockaddr*)&addr,
        sizeof(addr)
    );

    if (res < 0) {
        close(socket_fd);
        return -1; // Cannot bind socket
    }

    res = listen(socket_fd, SOMAXCONN);

    if (res < 0) {
        close(socket_fd);
        return -1; // Cannot listen on socket
    }
    
    return socket_fd;
}

struct Client {
    int fd; // File descriptor for the client socket
    sockaddr_storage addr; // Address of the client
    socklen_t addr_len; // Length of the address structure

    
};

struct Pollvec {
    // 0-th polldf is always the listening socket.
    // Other pollfds are clients.
    vector<pollfd> pollfds;

    ~Pollvec() {
        for (const auto& pfd : pollfds) {
            if (pfd.fd >= 0) {
                close(pfd.fd);
            }
        }
    }

    void set_listener(uint16_t listen_port) {
        int socket_fd = ipv6_enabled_sock(listen_port);
        if (socket_fd < 0) {
            socket_fd = ipv4_only_sock(listen_port);
        }
        if (socket_fd < 0) {
            // TODO: errors
            throw runtime_error("Cannot create listening socket");
        }
        if (fcntl(socket_fd, F_SETFL, O_NONBLOCK)) {
            close(socket_fd);
            // TODO: handle errors
            throw runtime_error("Cannot set listening socket to non-blocking mode");
        }
        pollfd listen_fd{};
        listen_fd.fd = socket_fd;
        listen_fd.events = POLLIN;
        listen_fd.revents = 0;
        pollfds.push_back(listen_fd);
    }

    void add_client(pollfd client) {
        pollfds.push_back(client);
    }
    void delete_client(size_t i) {
        // TODO: zamknij socket czy cos
        swap(pollfds[i], pollfds[pollfds.size() - 1]);
        pollfds.pop_back();
    }
    size_t size() {
        return pollfds.size();
    }

    void accept_new_connection() {
        if ((pollfds[0].revents & POLLIN) == 0) {
            return;
        }

        // TODO: zpaytaj Kamila czy tez uzywa tego storage
        Client client;
        client.addr_len = sizeof(client.addr);
        client.fd = accept(
            pollfds[0].fd, 
            (sockaddr*)&client.addr, 
            &client.addr_len
        );
        if (client.fd < 0) {
            // TODO: handle errors
            throw runtime_error("Cannot accept new connection");
        }
        if (fcntl(client.fd, F_SETFL, O_NONBLOCK)) {
            close(client.fd);
            // TODO: errors
            throw runtime_error("Cannot set client socket to non-blocking mode");
        }
        // TODO: cos wyslac jak zaczynam komunikacje? jakis timestamp?

        pollfd client_fd_struct{};
        client_fd_struct.fd = client.fd;
        client_fd_struct.events = POLLIN; 
        client_fd_struct.revents = 0;
        add_client(client_fd_struct);
    }
};