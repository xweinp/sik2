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
size_t get_n_small_letters(const string& str);
string to_proper_rational(double val);
    
using TimePoint = steady_clock::time_point;
using Msg = std::pair<TimePoint, string>;
struct MsgComparator {
    bool operator()(const Msg& a, const Msg& b) const {
        return a.first > b.first; 
    }
};


string make_penalty(const string &point, const string &value) {
    return "PENALTY " + point + " " + value + "\r\n"; 
}
string make_bad_put(const string &point, const string &value) {
    return "BAD_PUT " + point + " " + value + "\r\n";
}

string make_coeff(ifstream &file) {
    string res;
    getline(file, res); // TODO: ygh czy mam zakladac ze to moze zwrocic blad?
    return res;
}

string make_state(const vector<double> &approx) {
    string res = "STATE ";
    for (double val : approx) {
        res += to_proper_rational(val) + " ";
    }
    res += "\r\n";
    return res;
}


auto time_diff(TimePoint begin, TimePoint end) {
    return duration_cast<milliseconds>(end - begin).count();
}

struct MessageQueue {
    priority_queue<Msg, vector<Msg>, MsgComparator> messages; 
    size_t top_message_pos = 0; 

    void push(const string& msg, int delay_s) {
        auto now = steady_clock::now();
        auto time_to_send = now + seconds(delay_s);
        messages.push({time_to_send, msg});
    }
    void pop() {
        messages.pop();
    }
    bool empty() const {
        return messages.empty();
    }
    bool ready_message() const {
        if (messages.empty()) {
            return false;
        }
        auto now = steady_clock::now();
        return messages.top().first <= now;
    }
    Msg get_ready_message() {
        return messages.top();
    }
    // Returns: -1 iff error, 1 iff the whole message was sent, 0 otherwise
    int send_message(int socket_fd) {
        auto msg = get_ready_message().second;
        int sent_len = write(
            socket_fd, 
            msg.c_str() + top_message_pos,
            msg.size() - top_message_pos
        );
        if (sent_len < 0) {
            print_error("Cannot send message to client. errno: " + to_string(errno));
            return -1;
        }
        top_message_pos += sent_len;
        if (top_message_pos == msg.size()) {
            // We have sent the whole message.
            pop();
            top_message_pos = 0; // Reset position for the next message.
            return 1;
        }
        return 0;
    }
};

bool is_integer(const string& str); // DONE
bool is_proper_rational(const string& str); // DONE
tuple<string, string> get_point_value(const string& msg); // DONE
bool is_put(const string& msg); // DONE
// I assume that the integer non-negative
int get_int(const string& msg, int mx); // DONE
double get_double(const string& msg); // DONE
// This function assumes that the message is a PUT message checked by is_put.
bool is_bad_put(const string& point, const string& value, unsigned int k); // DONE



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
    void print_error_bad_message(const string& msg) {
        print_error(
            "bad message from " + 
            to_string_w_id() +
            ": " + msg
        );
    }
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
            if (pfd.fd >= 0) {
                close(pfd.fd);
            }
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

    void delete_client(size_t i) {
        counter_m -= players[i].n_proper_puts; 
        pollvec.delete_client(i);
        players.delete_client(i);
    }

    void play_a_game() {
        counter_m = 0;
        TimePoint next_event = steady_clock::now() + seconds(10); // Joke
        bool has_next_event = false;
        bool instant_event = false;

        while (counter_m < m and !(*finish_flag)) {
            if (next_event <= steady_clock::now()) {
                has_next_event = false;
            }
            int timeout = -1;
            if (has_next_event) {
                timeout = max(0, (int)time_diff(steady_clock::now(), next_event));
            }
            int poll_status = poll(
                pollvec.pollfds.data(), 
                (nfds_t) pollvec.size(),
                timeout
            );

            if (poll_status < 0) {
                print_error("Poll error occurred. errno: " + to_string(errno) + ".");
                if (*finish_flag) {
                    return; 
                }
                continue; 
            }
            // Poll status >= 0. 
            // I can have some events POLLIN or POLLOUT.
            // I can also have timeout due to a messege I'm supposed to send right now.

            TimePoint new_next_event = steady_clock::now() + seconds(10);

            // First I accept new connection. (if there is any)
            accept_new_connection();
            pollvec[0].revents = 0; 
            
            for (size_t i = 1; i < pollvec.size(); ++i) {
                auto &pollfd = pollvec[i]; 
                auto &client = players[i];
                
                if ((pollfd.revents & (POLLIN | POLLERR))) {
                    // read
                    ssize_t read_len = read(
                        pollfd.fd,
                        buffer.data(),
                        buff_len
                    );

                    if (read_len < 0) {
                        print_error(
                            "Reading message from " + 
                            client.ip + ":" + to_string(client.port) + 
                            " result in error. Closing connection"
                        );
                        delete_client(i);
                        --i;
                        continue; 
                    }
                    else if (read_len == 0) {
                        cout << "Client " << client.to_string_w_id() << " disconnected." << endl;
                        delete_client(i);
                        --i;
                        continue; 
                    }
                    else {
                        int read_res = client.read_message(buffer.substr(0, read_len), k, file);
                        if (read_res == - 1) {
                           delete_client(i);
                            --i;
                            continue;
                        }
                        else if (read_res == 1) {
                            // A proper PUT was made.
                            ++counter_m;
                            if (counter_m == m) {
                                // TODO: zakoncz gre
                            }
                        }
                    }

                }
                if ((pollfd.revents & POLLOUT)) {
                    while (client.has_ready_message_to_send()) {
                        if (client.send_message() != 1) {
                            // It reutns 1 iff the whole message was sent.
                            break;
                        }
                    }
                }
                pollfd.revents = 0;
                pollfd.events = POLLIN; // Reset events to POLLIN for the next poll 
                has_next_event = false;

                if (!client.helloed) {
                    if (client.connected_timestamp + seconds(3) <= steady_clock::now()) {
                        cout << "Client " << client.to_string_w_id() 
                             << " did not send HELLO in time. Disconnecting." << endl;
                        delete_client(i);
                        --i;
                        continue; 
                    }

                    new_next_event = min(
                        new_next_event, 
                        client.connected_timestamp + seconds(3)
                    );
                    has_next_event = true; 
                }   

                if (!client.messages_to_send.empty()) {
                    if (client.has_ready_message_to_send()) {
                        // If there are messages to send, set POLLOUT
                        pollfd.events |= POLLOUT; 
                    }
                    pollfd.events |= POLLOUT; // Set POLLOUT if there are messages to send
                    new_next_event = min(
                        new_next_event, 
                        client.messages_to_send.get_ready_message().first // First message to send
                    );
                    has_next_event = true; 
                }    
            }

            next_event = new_next_event;
        }
    }
};