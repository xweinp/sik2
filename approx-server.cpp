#include <iostream>
#include <cstdint>
#include <cstring>
#include <map>
#include <stdexcept>
#include <poll.h>
#include <unordered_set>

#include "utils.hpp"
#include "utils-server.hpp"

using namespace std;

const uint64_t DEF_P = 0, MIN_P = 0, MAX_P = 65535;
const uint64_t DEF_K = 100, MIN_K = 1, MAX_K = 10000;
const uint64_t DEF_N = 4, MIN_N = 1, MAX_N = 8;
const uint64_t DEF_M = 131, MIN_M = 1, MAX_M = 12341234;

// TODO: co na cerr co na cout???
// TODO: erorry jak w tresi

static bool finish = false;

static void catch_int(int sig) {
    finish = true;
    printf("signal %d catched so no new connections will be accepted\n", sig);
}

int main(int argc, char* argv[]) {    
    map<char, char*> args;

    if (argc % 2 != 1) {
        cout << "ERROR: Every option must have a value.\n";
        return 1;
    }

    unordered_set<string> valid_args = {
        "-p", "-k", "-n", "-m", "-f"
    };
    
    for (int i = 1; i < argc; i += 2) {
        if (!valid_args.contains(argv[i])) {
            cout << "ERROR: invalid option " << argv[i] << ".\n";
            return 1;
        }
        if (args.contains(argv[i][1])) {
            cout << "ERROR: double parameter " << argv[i] << ".\n";
            return 1;
        }
        args[argv[i][1]] = argv[i + 1];
    }

    uint16_t port;
    uint16_t k;
    uint8_t n;
    uint32_t m;
    char *f = NULL;
    uint32_t f_len;

    try {
        port = get_arg('p', args, DEF_P, MIN_P, MAX_P);
        k = get_arg('k', args, DEF_K, MIN_K, MAX_K);
        n = get_arg('n', args, DEF_N, MIN_N, MAX_N);
        m = get_arg('m', args, DEF_M, MIN_M, MAX_M);
    }
    catch (const invalid_argument &e) {
        cout << "ERROR: " << e.what();
        return 1;
    } 

    if (!args.contains('f')) {
        cout << "ERROR: option -f is mandatory.\n";
        return 1; 
    }
    f = args['f'];

    Pollvec pollfds;
    PlayerSet players;
    string buffer;

    try {
        install_signal_handler(SIGINT, catch_int, SA_RESTART);
        pollfds.set_listener(port);
        buffer.resize(1024 * 1024); // TODO: size management
    }
    catch (const runtime_error &e) {
        // TODO: handle errors
        cout << "ERROR: " << e.what() << "\n";
        return 1;
    }

    do {
        if (finish) {
            // TODO: close all sockets or idk
        }
        int poll_status = poll(
            pollfds.pollfds.data(), 
            (nfds_t) pollfds.size(), 
            -1 // TODO: time management
        );
        if (poll_status < 0) {
            // TODO: wtf is oging on here?
            continue;
        }
        else if (poll_status == 0) {
            // TODO: also wtf
            continue;
        }
        
        // New connection.
        try {
            pollfds.accept_new_connection();
        }
        catch (const runtime_error &e) {
            cout << "ERROR: " << e.what() << "\n";
            continue;
        }
        for (int i = 1; i < pollfds.size(); ++i) {
            if (pollfds.pollfds[i].revents & POLLIN) {
                // I can read from this buffers socket.

                auto &player = players.get_player(i);
                size_t len = read(
                    player.fd, 
                    buffer.data(), 
                    buffer.size()
                );
                if (len < 0) {
                    // TODO: handle errors
                }
                if (len == 0) {
                    // Client disconnected.
                    cout << "Client disconnected.\n";
                    players.delete_client(i);
                    pollfds.delete_client(i);
                    continue;
                }
                player.extend_message(buffer.substr(0, len));
            
                if (!player.helloed) {

                }
                    
            }
            if (pollfds.pollfds[i].revents & POLLOUT) {
                // I can write if there is something to write.
            }
        }

    } while (true);

    return 0;
}