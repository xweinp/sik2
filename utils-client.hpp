#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <poll.h>
#include <queue>

using namespace std;

struct ClientMessageQueue {
    queue<string> messages;
    string current_message;
    size_t current_pos = 0;

    void push(const string& msg) {
        messages.push(msg);
    }
    bool empty() const {
        return messages.empty();
    }
    void send_message(int socket_fd) {
        if (current_message.empty()) {
            current_message = messages.front();
            messages.pop();
            current_pos = 0;
        }

        ssize_t bytes_sent = write(
            socket_fd, 
            current_message.c_str() + current_pos, 
            current_message.size() - current_pos
        );
        if (bytes_sent < 0) {
            print_error("Senging message failed: " + string(strerror(errno)));
            return;
        }
        current_pos += bytes_sent;
        if (current_pos == current_message.size()) {
            current_message.clear();
            current_pos = 0;
        }
    }
};

bool valid_coeff(string &msg) {
    bool pref_suf = msg.size() > 8 and 
        msg.substr(0, 6) == "COEFF " and
        msg.substr(msg.size() - 2, 2) == "\r\n";
    if (!pref_suf) {
        return false;
    }

    msg.pop_back();
    msg.pop_back(); // Remove "\r\n"

    for (size_t i = 6; i < msg.size();) {
        size_t next_space = msg.find(' ', i);
        string coeff = msg.substr(i, next_space - i);
        if (coeff.empty() || !is_proper_rational(coeff)) {
            msg += "\r\n"; 
            return false; // Invalid coefficient format
        }
        i = next_space + 1;
    }
    msg += "\r\n"; 
    return true; // Valid COEFF message
}


struct Client {
    string player_id;
    string server_address;
    uint32_t server_port;
    string server_ip;
    int socket_fd = -1;

    ClientMessageQueue messages_to_send;
    string receinved_buffer;
    const size_t buff_len = 5000;
    string buffer;

    pollfd fds[2]; // fds[0] is for stdin, fds[1] is for the server socket

    Client(string player_id, string server_address, uint32_t server_port)
        : player_id(player_id), server_address(server_address), 
        server_port(server_port), buffer(buff_len, '\0') {}


    // returns -1 on error
    int setup_stdin();
    // returns -1 on error
    int connect_to_server(bool force_ipv4, bool force_ipv6);
    // returns -1 on error
    int send_hello();

    // Before I read anything I must get COEFF
    int get_coeff() {

    }

    int read_message() {
        size_t read_len = read(fds[1].fd, buffer.data(), buff_len);
        if (read_len < 0) {
            print_error("Reading message from server failed: " + string(strerror(errno)));
            return -1;
        }
        else if (read_len == 0) {
            cout << "Server closed the connection." << endl;
            return 1;
        }
        receinved_buffer += string(buffer.data(), read_len);

    }

    void auto_play() {
    
    }
    void interactive_play() {
        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;
        fds[1].fd = socket_fd;
        fds[1].events = POLLIN | POLLOUT;

        bool got_coeff = false;
        bool finished = false;
        while (!finished) {
            
        }
    }
};



bool check_mandatory_option(const auto &args, char option) {
    if (!args.contains(option)) {
        cout << "ERROR: option -" << option << " is mandatory.\n";
        return false;
    }
    return true;
}

