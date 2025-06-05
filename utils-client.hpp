#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

// returns socket file descriptor or -1 on error
int connect_to_server(bool force_ipv4, bool force_ipv6, const string &server_address, uint16_t port) {
    addrinfo hints;
    addrinfo *res;

    hints.ai_family = force_ipv4 ? AF_INET : (force_ipv6 ? AF_INET6 : AF_UNSPEC);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int ret = getaddrinfo(
        server_address.c_str(),
        to_string(port).c_str(),
        &hints,
        &res
    );

    if (ret != 0) {
        cout << "ERROR: getaddrinfo failed: " << gai_strerror(ret) << "\n";
        // TODO: handle errors properly
        return -1;
    }

 
    int socket_fd = socket(
        res->ai_family,
        SOCK_STREAM, 
        0
    );
    if (socket_fd < 0) {
        freeaddrinfo(res);
        cout << "ERROR: Cannot create socket.\n";
        // TODO: handle errors properly
        return -1;
    }

    int res = connect(
        socket_fd,
        res->ai_addr,
        res->ai_addrlen
    );
    if (res < 0) {
        freeaddrinfo(res);
        cout << "ERROR: Cannot connect to server.\n";
        // TODO: handle errors properly
        close(socket_fd);
        return -1;
    }

    cout << "Connected to server at " << server_address << ":" << port << "\n";
    // TODO: better couts
    freeaddrinfo(res);

    return socket_fd;
}


bool check_mandatory_option(const auto &args, char option) {
    if (!args.contains(option)) {
        cout << "ERROR: option -" << option << " is mandatory.\n";
        return false;
    }
    return true;
}

void send_hello() {

}