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

struct Message {
    string content;
    int delay_s; 
    int send_weigth;

    bool operator<(const Message& other) const {
        return delay_s < other.delay_s and send_weigth < other.send_weigth;
    }
    void make_penalty() {
        // TODO: fix this!
        content = "PENALTY\r\n";
        delay_s = 0;
        send_weigth = 0;
    }
    void make_state() {
        content = "STATE\r\n";
        // delay_s = 
        // TODO: wtf???
    }
};

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
    string get_ready_message() {
        return messages.top().second;
    }
    void send_message(int socket_fd) {
        auto msg = get_ready_message();
        int sent_len = write(
            socket_fd, 
            msg.c_str() + top_message_pos,
            msg.size() - top_message_pos
        );
        if (sent_len < 0) {
            cout << "ERROR: Cannot send message to client.\n";
            // TODO: co w takim errorze?
            return;
        }
        top_message_pos += sent_len;
        if (top_message_pos == msg.size()) {
            // We have sent the whole message.
            pop();
            top_message_pos = 0; // Reset position for the next message.
        }   
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

string make_penalty(); // TODO: implement me!
string make_state(); // TODO: implement me!
string make_bad_put(); // TODO: implement me!
string make_coeff(); // TODO: implement me!

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

    void read_message(const string& msg, int k) {
        // every message ends with "\r\n"
        size_t old_len = buffered_message.size();
        buffered_message += msg;
        size_t erase_pref = 0;
        vector<string> messages;

        for (size_t i = old_len > 0? old_len - 1: 0; i + 1 < buffered_message.size(); ++i) {
            if (buffered_message[i] == '\r' and buffered_message[i + 1] == '\n') {
                // Found end of a message.
                messages.push_back(buffered_message.substr(erase_pref, i + 2 - erase_pref));
                erase_pref = i + 2; // Erase everything before this point.
            }
        }
        buffered_message.erase(0, erase_pref);

        if (messages.empty()) {
            if (buffered_message.empty()) {
                started_before_reply = false;
            }
            else if (messages_to_send.empty()) {
                // We have not sent any replies yet.
                started_before_reply = true;
            }
            started_before_reply |= !(buffered_message.empty() or messages_to_send.empty());
            return;
        }
        // There is at lest one full message
        const string& first_message = messages[0];

        if (!messages_to_send.empty()) {
            // We have not sent all replies yet.
            started_before_reply = true;
        }

        if (!helloed) {
            // First message must be a HELLO.
            if (!proper_hello(first_message)) {
                cout << "ERROR: First message is not a proper HELLO.\n";
                // TODO: handle errors properly.
                // TODO: i think i have to disconnect the client here.
            }
            else {
                // First message is a proper HELLO.
                id = id_from_hello(first_message);
                n_small_letters = get_n_small_letters(id);
                helloed = true;
                
                // TODO: some log about new player?
                cout << "New player: " << id << "\n";
                messages_to_send.push(make_coeff(), 0);
            }

        }
        else if (!is_put(first_message)) {
            // This is not even a proper PUT message.
            // I just print ERROR and ignore it.
            // TODO: handle errors properly.
            cout << "ERROR: Message is not a PUT message.\n";
            started_before_reply = false; // Reset the flag.
        }
        else if (started_before_reply) {
            // First message started before we got a reply.
            
            // First message is a PUT message.
            // It was sent before the player received a reply so I resend penalty.
            messages_to_send.push(make_penalty(), 0);
            // If the message is a bad put then we also send bad_put.
            auto [point, value] = get_point_value(first_message);
            if (is_bad_put(point, value, k)) {
                messages_to_send.push(make_bad_put(), 1);
            }
            started_before_reply = false; // Reset the flag.
        }
        else {
            // First message is a PUT message.
            // If the message is a bad put then we also send bad_put.
            auto [point, value] = get_point_value(first_message);
            if (is_bad_put(point, value, k)) {
                messages_to_send.push(make_bad_put(), 1);
            }
            else {
                messages_to_send.push(make_state(), n_small_letters);
            }
        }

        for (int i = 1; i < messages.size(); ++i) {
            
            const string& msg = messages[i];
            if (!is_put(msg)) {
                // This is not even a proper PUT message.
                // I just print ERROR and ignore it.
                // TODO: handle errors properly.
                cout << "ERROR: Message is not a PUT message.\n";
                continue; // Ignore this message.
            }
            auto [point, value] = get_point_value(msg);
            if (is_bad_put(point, value, k)) {
                messages_to_send.push(make_bad_put(), 1);
            }
            if (messages_to_send.empty()) {
                // If the message is a bad put then we also send bad_put.
                messages_to_send.push(make_state(), n_small_letters);
            }
            else {
                // We have already sent a reply.
                messages_to_send.push(make_penalty(), 0);
            }
        }
        if (!buffered_message.empty() and !messages_to_send.empty()) {
            // We have some buffered message and we have sent a reply.
            started_before_reply = true;
        }
    }
    bool has_ready_message_to_send() const {
        return !messages_to_send.ready_message();
    }
    void send_message() {
        messages_to_send.send_message(fd);
    }
};


struct PlayerSet {
    vector<Client> players;

    void delete_client(size_t i) { //INDEXING FORM 1! 
        swap(players[i - 1], players[players.size() - 1]);
        players.pop_back();
    }
    Client &get_player(size_t i) { //INDEXING FORM 1! 
        return players[i - 1];
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

    int set_up() {
        int socket_fd = ipv6_enabled_sock(listen_port);
        if (socket_fd < 0) {
            socket_fd = ipv4_only_sock(listen_port);
        }
        if (socket_fd < 0) {
            print_error("Cannot create listening socket.");
            return -1;
        }
        if (fcntl(socket_fd, F_SETFL, O_NONBLOCK)) {
            close(socket_fd);
            print_error("Cannot set listening socket to non-blocking mode.");
            return -1;
        }
        pollfd listen_fd{};
        listen_fd.fd = socket_fd;
        listen_fd.events = POLLIN;
        listen_fd.revents = 0;
        pollfds.push_back(listen_fd);
        return 0;
    }

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


struct Server {
    Pollvec pollvec;
    PlayerSet players;
    int32_t k, n;
    int32_t m, counter_m = 0;
    char* filename;
    bool *finish_flag; 
    ifstream file;
    
    Server(
        int32_t listen_port,  int32_t k, 
        int32_t n, int32_t m, char *filename,
        bool *finish_flag
    ) : pollvec(listen_port), k(k), m(m), n(n), 
        filename(filename), finish_flag(finish_flag) {} 

    int set_up() {
        if (pollvec.set_up()) {
            // polvec prints error
            return -1; 
        }
        file.open(filename);
        if (!file) {
            print_error("Cannot open file: " + string(filename));
            return -1;
        }
        return 0;
    }

    void play_a_game() {
        counter_m = 0;
        TimePoint next_event = steady_clock::now() + years(10); // Joke
        bool has_next_event = false;
        bool instant_event = false;

        while (counter_m < m and !(*finish_flag)) {
            if (next_event <= steady_clock::now()) {
                has_next_event = false;
            }
            
            int poll_status = poll(
                pollvec.pollfds.data(), 
                (nfds_t) pollvec.size(),
                has_next_event? time_diff(steady_clock::now(), next_event) : -1
            );

            if (poll_status < 0) {
                print_error("Poll error occurred. errno: " + to_string(errno) + ".");
                if (*finish_flag) {
                    return; 
                }
            }

            else if (poll_status == 0) { // hmm i probably have something to send
            }

        }
    }
};