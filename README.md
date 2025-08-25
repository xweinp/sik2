# Approximation Game (Client-Server)

A simple C++20 client–server game where multiple clients try to approximate a hidden function managed by the server. The server streams information (coefficients / state / penalties) and evaluates how close each player’s evolving approximation is, while clients submit updates (PUT messages) either interactively or automatically.

## Features
- Single server, multiple concurrent players (poll-based I/O)
- IPv4 & IPv6 support (force with `-4` / `-6` on client)
- Interactive or automatic client strategy (`-a`)
- Simple text protocol: HELLO, coefficient/state/bad_put/penalty/scoring style messages
- Graceful partial send handling with internal message queues (both sides)

## Build
Requires a C++20 compiler (e.g., g++). Build both binaries:
```
make
```
Artifacts: `approx-server`, `approx-client`

Clean:
```
make clean
```

## Server Usage
```
./approx-server -f <coeff_file> [-p <port>] [-k <k>] [-n <n>] [-m <m>]
```
Options (defaults from code):
- `-f <coeff_file>`  (mandatory) file providing coefficients / data the server serves
- `-p <port>`        listening port (0–65535, default 0 -> ephemeral)
- `-k <k>`           approximation order / size parameter (default 100)
- `-n <n>`           internal timing / game parameter (default 4)
- `-m <m>`           scoring / cycle limit parameter (default 131)

Server loops: runs a game, emits scoring, then starts a new one after a short pause.

## Client Usage
```
./approx-client -u <player_id> -s <server_host> -p <port> [-a] [-4] [-6]
```
Options:
- `-u <player_id>`   player identifier (validated: certain length/charset)
- `-s <server_host>` server DNS name or IP
- `-p <port>`        server port
- `-a`               enable automatic play strategy
- `-4` / `-6`        force IPv4 / IPv6 (cannot combine; both -> ignored)

Interactive mode reads commands from stdin (e.g., PUT lines). Auto mode drives itself.

## Protocol (High-Level Glimpse)
- Client sends HELLO with its ID.
- Server replies with coefficients and state messages over time.
- Clients send PUT updates attempting to refine an approximation vector.
- Server may respond with `bad_put` or `penalty` when inputs are invalid or early.
- Periodic `state` and final `scoring` messages summarize progress / error.

(See source in `client/` and `server/` plus shared helpers in `common/` for exact rules.)

## Development Notes
- Both sides use non-blocking sockets + `poll` for multiplexing.
- Message fragmentation is handled: queues track current position; partial writes retry.
- Add new message types by extending validation in `utils-client.*` / `utils-server.*`.

## Quick Start Example
```
# Terminal 1
./approx-server -p 5555 -k 10 -n 4 -m 200 -f coeffs.txt

# Terminal 2
./approx-client -u player1 -s localhost -p 5555 -a
```