# Chat Room Application

A simple terminal-based chat application with client and server components.

## Prerequisites

- C++ compiler with C++11 support
- Make (optional, but recommended)
- On Windows: MinGW or Visual Studio with C++ support

## Building the Project

### Using Make (Recommended)

```bash
# On macOS/Linux
make

# On Windows (with MinGW)
mingw32-make
```

### Manual Compilation

#### On macOS/Linux
```bash
# Compile the server
g++ -std=c++11 -Wall -pthread -o server server.cpp

# Compile the client
g++ -std=c++11 -Wall -pthread -o client client.cpp
```

#### On Windows
```bash
# Compile the server
g++ -std=c++11 -Wall -pthread -o server.exe server.cpp -lws2_32

# Compile the client
g++ -std=c++11 -Wall -pthread -o client.exe client.cpp -lws2_32
```

## Running the Application

### 1. Start the Server First

```bash
# On macOS/Linux
./server

# On Windows
server.exe
```

### 2. Run the Client(s)

You can run multiple clients to connect to the server.

```bash
# On macOS/Linux
./client

# On Windows
client.exe
```

3. Enter your name when prompted
4. Start chatting!

## Commands

- Type `#exit` to leave the chat room

## Notes

- The server listens on port 10000 by default
- By default, the client connects to INADDR_ANY. To connect to a specific server, uncomment and modify the line with `inet_addr("127.0.0.1")` in client.cpp
- The application supports colored text for different users in the terminal
- You can run multiple clients to simulate a chat room environment

## Troubleshooting

If you encounter compilation errors:

1. On macOS, we use `::bind()` instead of just `bind()` to avoid conflicts with the C++ standard library.
2. We use `std::move()` instead of just `move()` to properly qualify the function.
3. We use `memset()` instead of `bzero()` with a size of 0, which can cause warnings.
4. For Windows compatibility, we include the Winsock2 library and handle platform-specific socket initialization. 