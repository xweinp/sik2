#pragma once

#include <poll.h>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> 
#include <fcntl.h> 
#include <queue>
#include <iostream>
#include <fstream>

#include "utils.hpp"

using namespace std;

int ipv6_enabled_sock(uint16_t port);
int ipv4_only_sock(uint16_t port);

bool proper_hello(const string& msg);
string id_from_hello(const string& msg);
size_t get_no_small_letters(const string& str);

string make_penalty(const string &point, const string &value);
string make_bad_put(const string &point, const string &value);
string make_coeff(ifstream &file);
string make_state(const vector<double> &approx);


bool is_integer(const string& str);
bool is_proper_rational(const string& str);
tuple<string, string> get_point_and_value(const string& msg);
bool is_put(const string& msg);
// I assume that the integer non-negative
int get_int(const string& msg, int mx);
double get_double(const string& msg); 
// This function assumes that the message is a PUT message checked by is_put.
bool is_bad_put(const string& point, const string& value, unsigned int k); // DONE
    
using TimePoint = steady_clock::time_point;
using Msg = std::pair<TimePoint, string>;
struct MsgComparator {
    bool operator()(const Msg& a, const Msg& b) const {
        return a.first > b.first; 
    }
};
auto time_diff(TimePoint begin, TimePoint end) {
    return duration_cast<milliseconds>(end - begin).count();
}

struct MessageQueue {
    priority_queue<Msg, vector<Msg>, MsgComparator> messages; 
    size_t current_pos = 0;
    string current_message; // TODO: napraw zeby sie nie mieszaly te wiadomosci!

    void push(const string& msg, int delay_s);
    void get_current();
    bool currently_sending() const;
    bool empty() const;
    bool ready_message() const;
    // Returns: -1 iff error, 1 iff the whole message was sent, 0 otherwise
    int send_message(int socket_fd);
    TimePoint get_ready_time() const;
    void send_scoring(const string &scoring, int socket_fd);
};





struct Client {
    int fd; // File descriptor for the client socket
    sockaddr_storage addr; // Address of the client
    socklen_t addr_len; // Length of the address structure

    string id;
    size_t n_small_letters = 0; // Number of small letters in the id
    string buffered_message;
    bool started_before_reply = 0;
    
    string reply_buffer; // Buffer for the reply to the client
    size_t reply_pos = 0; // Position in the reply buffer

    bool helloed = 0;

    MessageQueue messages_to_send;
    string current_message;
    size_t current_message_pos = 0;

    string ip;
    int32_t port;

    int32_t n_proper_puts = 0; 

    TimePoint connected_timestamp;
    
    vector<double> approx;
    vector<double> goal;
    double error = 0.0; 

    Client(size_t k) : approx(k + 1, 0.0) {}

    int set_port_and_ip();
    // returns: -1 iff we should disconnect the client, 1 iff a proper put was made, 0 otherwise
    int read_message(const string& msg, int k, ifstream &file);
    
    bool has_ready_message_to_send() const {
        return !messages_to_send.ready_message();
    }
    // Returns: -1 iff error, 1 iff the whole message was sent, 0 otherwise
    int send_message() {
       return messages_to_send.send_message(fd);
    }
    string to_string_w_id() { // with id
        return ip + ":" + to_string(port) + ", " + id != ""? id : "UNKNOWN";
    }
    string to_string_wo_id() {
        return ip + ":" + to_string(port);
    }
    void print_error_bad_message(const string& msg);
    void send_scoring(const string &scoring) {
        messages_to_send.send_scoring(scoring, fd);
    }
    vector<double> parse_coefficients(const string &coeff_str);
    void calc_goal_from_coef(const string &coeff);
    void update_approximation(const string &point, const string &value);
};


struct PlayerSet {
    vector<Client> players;

    PlayerSet() : players(1) {} // Because pollfd[0] is the listening socket

    void delete_client(size_t i) {
        swap(players[i], players[players.size() - 1]);
        players.pop_back();
    }
    Client& operator[](size_t i) {
        return players[i];
    }
    void add_player(const Client &client) {
        players.push_back(client);
    }
};


struct Pollvec {
    // 0-th polldf is always the listening socket.
    // Other pollfds are clients.
    vector<pollfd> pollfds;
    int32_t listen_port;
    Pollvec(int32_t _listen_port) : listen_port(_listen_port) {};
    ~Pollvec() {
        for (const auto& pfd : pollfds) {
            close(pfd.fd);
        }
    }

    pollfd& operator[](size_t i) {
        return pollfds[i];
    }

    int set_up();

    void add_client(pollfd client) {
        pollfds.push_back(client);
    }
    void delete_client(size_t i) {
        close(pollfds[i].fd);
        swap(pollfds[i], pollfds[pollfds.size() - 1]);
        pollfds.pop_back();
    }
    size_t size() {
        return pollfds.size();
    }
};


struct Server {
    Pollvec pollvec;
    PlayerSet players;
    int32_t k, n;
    int32_t m, counter_m = 0;
    char* filename;
    bool *finish_flag; 
    ifstream file;
    const size_t buff_len = 5000;
    string buffer;
    
    Server(
        int32_t listen_port,  int32_t k, 
        int32_t n, int32_t m, char *filename,
        bool *finish_flag
    ) : pollvec(listen_port), k(k), m(m), n(n), 
        filename(filename), finish_flag(finish_flag), 
        buffer(buff_len, '\0') {} 

    int set_up();
    void accept_new_connection();
    void delete_client(size_t i);
    string make_scoring();
    void finish_game();
    void play_a_game();
};