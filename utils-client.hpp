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

bool check_mandatory_option(const map<char, char*> &args, char option);

bool valid_bad_put(const string &msg);
bool valid_penalty(const string &msg);
bool valid_state(const string &msg);
bool valid_coeff(string &msg);

struct ClientMessageQueue {
    queue<string> messages;
    string current_message;
    size_t current_pos = 0;

    void push(const string& msg);
    bool empty() const;
    void send_message(int socket_fd);
};




struct Client {
    string player_id;
    string server_address;
    uint16_t server_port;
    string server_ip;
    int socket_fd = -1;
    int32_t k, n;


    ClientMessageQueue messages_to_send;
    string received_buffer;
    const size_t buff_len = 5000;
    string buffer;

    bool got_coeff = false;
    bool got_response = false;
    vector<double> coefficients;



    pollfd fds[2]; // fds[0] is for stdin, fds[1] is for the server socket

    Client(string _player_id, string _server_address, uint16_t _server_port)
        : player_id(_player_id), server_address(_server_address), 
        server_port(_server_port), buffer(buff_len, '\0') {}


    // returns -1 on error
    int setup_stdin();
    // returns -1 on error
    int connect_to_server(bool force_ipv4, bool force_ipv6);
    // returns -1 on error
    int send_hello();


    // returns -1 on error, 1 if the game ended, 0 otherwise
    int read_message();
    int read_from_stdin();

    // Returns -1 on error
    int auto_play();
    // Returns -1 on error
    int interactive_play();
};
