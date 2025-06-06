#include "utils.hpp"
#include "utils-client.hpp"

int Client::setup_stdin() {
    int fd = STDIN_FILENO;
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        print_error("Cannot get flags for stdin: " + string(strerror(errno)));
        return -1;
    }
    flags |= O_NONBLOCK; 
    if (fcntl(fd, F_SETFL, flags) < 0) {
        print_error("Cannot set stdin to non-blocking: " + string(strerror(errno)));
        return -1;
    }
}

int Client::connect_to_server(bool force_ipv4, bool force_ipv6) {
    addrinfo hints;
    addrinfo *res;

    hints.ai_family = force_ipv4 ? AF_INET : (force_ipv6 ? AF_INET6 : AF_UNSPEC);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int ret = getaddrinfo(
        server_address.c_str(),
        to_string(server_port).c_str(),
        &hints,
        &res
    );

    if (ret != 0) {
        print_error("getaddrinfo failed: " + string(gai_strerror(ret)));
        return -1;
    }


    int socket_fd = socket(
        res->ai_family,
        SOCK_STREAM, 
        0
    );
    if (socket_fd < 0) {
        print_error("Cannot create socket: " + string(strerror(errno)));
        freeaddrinfo(res);
        return -1;
    }

    int res = connect(
        socket_fd,
        res->ai_addr,
        res->ai_addrlen
    );
    if (res < 0) {
        print_error("Cannot connect to server: " + string(strerror(errno)));
        freeaddrinfo(res);
        close(socket_fd);
        return -1;
    }

    fds[1].fd = socket_fd;
    fds[1].events = POLLIN | POLLOUT;
    fds[1].revents = 0;

    cout << "Connected to server at " << server_address << ":" << server_port << endl;
    freeaddrinfo(res);
    return socket_fd;
}

int Client::send_hello() {
    string message = "HELLO " + player_id + "\r\n";
    size_t pos = 0;
    while (pos < message.size()) {
        ssize_t bytes_sent = send(
            socket_fd, 
            message.c_str() + pos, 
            message.size() - pos, 
            0
        );
        if (bytes_sent < 0) {
            print_error("Error while sending HELLO message.");
            close(socket_fd);
            return -1;
        }
        pos += bytes_sent;
    }
}