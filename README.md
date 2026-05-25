# WebSocket Demo based on libwebsockets

A simple WebSocket server and client implementation using libwebsockets.

## Build

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

### Start the server

```bash
./ws_server
```

The server listens on `ws://localhost:8000`.

### Start the client

```bash
./ws_client
```

Type a message and press Enter to send. Type `quit` to exit.

The server broadcasts received messages to all connected clients.

## Dependencies

- libwebsockets
- CMake >= 3.10
- GCC / Clang (C11 support)
