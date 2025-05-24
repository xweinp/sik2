#include <poll.h>
#include <vector>

using namespace std;

struct pollvec {
    vector<pollfd> pollfds;

    pollvec() {
        // TODO: musze stworzyc gniazda dla łącznia dla ipv4 i ipv6
    }
    ~pollvec() {
        // TODO: pozamykaj gniazda!
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
};