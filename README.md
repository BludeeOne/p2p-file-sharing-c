# P2P File Sharing in C

A peer-to-peer file sharing system written in C with raw BSD sockets: a central **registry** server that tracks which peer has which files, and a **peer** client that joins the network, announces its files, searches for files, and fetches them from other peers.

Built for EECE 446 (Computer Networks) at CSU Chico, Spring 2025. The four programs build on each other across the semester, from a basic TCP client up to the full registry and peer system.

## How it works

```
        JOIN / PUBLISH / SEARCH
 peer ───────────────────────────▶ registry
  │                                   │  tracks peer IDs, addresses,
  │                                   │  and published file lists
  │      FETCH <filename>             │
  └─────────────────────────────▶ other peer
```

The registry is a single-threaded server that multiplexes all peer connections with `select()`, so it serves many peers without threads.

### Protocol

All messages are binary, and multi-byte integers are in network byte order.

| Action | Opcode | Payload | Response |
|---|---|---|---|
| JOIN | `0` | 4-byte peer ID | none |
| PUBLISH | `1` | 4-byte file count, then null-terminated filenames | none |
| SEARCH | `2` | null-terminated filename | 10 bytes: peer ID (4), IPv4 (4), port (2); all zeros if not found |
| FETCH | `3` | null-terminated filename (sent peer-to-peer) | 1-byte status, then file contents |

The peer sends reliably with a `send_all()` loop that handles partial writes, and reads its shared files from a local `SharedFiles/` directory.

## Build and run

```bash
cd program4
gcc -Wall -o registry registry.c
gcc -Wall -o peer peer.c

# terminal 1
./registry 5555

# terminal 2 (run from a directory containing SharedFiles/)
mkdir -p SharedFiles && echo hello > SharedFiles/a.txt
./peer localhost 5555 7
```

At the peer prompt:

```
Enter a command: JOIN
JOIN sent to registry.
Enter a command: PUBLISH
PUBLISH sent (1 files announced).
Enter a command: SEARCH
Enter a file name: a.txt
File found at
Peer 7
127.0.0.1:39506
Enter a command: EXIT
```

## Repo layout

| Folder | What it is |
|---|---|
| `program1/` | TCP client that fetches a web page over HTTP/1.0 in fixed-size chunks and counts `<h1>` tags |
| `program2/` | First peer client: JOIN, PUBLISH, SEARCH against the registry |
| `program3/` | Adds FETCH, downloading a file directly from another peer |
| `program4/` | My registry server (`select()`-based), plus the final peer |

## Limitations

- This peer implements the client side only. It requests files from other peers but doesn't accept incoming FETCH requests itself. FETCH was tested against the course's reference peer.
- The registry holds at most 5 peers with 10 files each, and stores the address each peer connected from.
- IPv4 only.

## Author

Erik Flores-Siemsen
