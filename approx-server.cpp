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

const int64_t DEF_P = 0, MIN_P = 0, MAX_P = 65535;
const int64_t DEF_K = 100, MIN_K = 1, MAX_K = 10000;
const int64_t DEF_N = 4, MIN_N = 1, MAX_N = 8;
const int64_t DEF_M = 131, MIN_M = 1, MAX_M = 12341234;

// TODO: co na cerr co na cout???
// TODO: erorry jak w tresi

static bool finish = false;

static void catch_int(int sig) {
    finish = true;
    print_error("Caught SIGINT, finishing the server.");
}

int main(int argc, char* argv[]) {
    if(install_signal_handler(SIGINT, catch_int, SA_RESTART)) {
        return 1;
    }

    map<char, char*> args;

    if (argc % 2 != 1) {
        print_error("Every option must have a value.");
        return 1;
    }

    unordered_set<string> valid_args = {
        "-p", "-k", "-n", "-m", "-f"
    };
    
    for (int i = 1; i < argc; i += 2) {
        if (!valid_args.contains(argv[i])) {
            print_error(string("Invalid option ") + argv[i] + ".");
            return 1;
        }
        if (args.contains(argv[i][1])) {
            print_error(string("Double parameter ") + argv[i] + ".");
            return 1;
        }
        args[argv[i][1]] = argv[i + 1];
    }

    int32_t port;
    int32_t k;
    int32_t n;
    int32_t m;
    char *f = NULL;

    port = get_arg('p', args, DEF_P, MIN_P, MAX_P);
    k = get_arg('k', args, DEF_K, MIN_K, MAX_K);
    n = get_arg('n', args, DEF_N, MIN_N, MAX_N);
    m = get_arg('m', args, DEF_M, MIN_M, MAX_M);
    
    if (port < 0 or k < 0 or n < 0 or m < 0) {
        return 1;
    }

    if (!args.contains('f')) {
        print_error("option -f is mandatory.");
        return 1; 
    }
    f = args['f'];

    Server server(port, k, n, m, f, &finish);


    do {
        if (finish) {
            // TODO: close all sockets or idk
        }
        
        if (poll_status ==) {
            // TODO: wtf is oging on here?
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
                player.read_message(buffer.substr(0, len), k);
            }
            if (pollfds.pollfds[i].revents & POLLOUT) {
                // I can write if there is something to write.
                auto &player = players.get_player(i);
                if (player.has_ready_message_to_send()) {
                    player.send_message();
                }
            }
        }
    } while (true);

    return 0;
}