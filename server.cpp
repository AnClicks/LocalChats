#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <algorithm>
#include <signal.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <process.h>
    #pragma comment(lib, "ws2_32.lib")
    #define bzero(b,len) (memset((b), '\0', (len)))
    #define close(s) closesocket(s)
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
    #include <pthread.h>
#endif

#define MAX_LEN 200
#define NUM_COLORS 6

using namespace std;

// Forward declarations
string color(int code);
void set_name(int id, char name[]);
void shared_print(string str, bool endLine = true);
int broadcast_message(string message, int sender_id);
int broadcast_message(int num, int sender_id);
void end_connection(int id);
void handle_client(int client_socket, int id);

#ifdef _WIN32
unsigned __stdcall client_thread(void* arg);
#else
void* client_thread(void* arg);
#endif

struct thread_data {
    int client_socket;
    int id;
};

struct client_info {
    int id;
    string name;
    int socket;
    #ifdef _WIN32
    HANDLE thread_handle;
    #else
    pthread_t thread;
    #endif
};

vector<client_info> clients;
int seed = 0;

#ifdef _WIN32
CRITICAL_SECTION cout_mtx, clients_mtx;
#else
pthread_mutex_t cout_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_mtx = PTHREAD_MUTEX_INITIALIZER;
#endif

int main() {
#ifdef _WIN32
    // Initialize critical sections
    InitializeCriticalSection(&cout_mtx);
    InitializeCriticalSection(&clients_mtx);
    
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed" << endl;
        return -1;
    }
#endif

    int server_socket;
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket: ");
        exit(-1);
    }

    // Set socket option to reuse address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(-1);
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(8888);  // Changed port to 8888
    server.sin_addr.s_addr = INADDR_ANY;
    memset(&server.sin_zero, '\0', sizeof(server.sin_zero));

    int bind_result = ::bind(server_socket, (struct sockaddr*)&server, sizeof(struct sockaddr_in));
    if (bind_result == -1) {
        perror("bind error");
#ifdef _WIN32
        cout << "WSA Error code: " << WSAGetLastError() << endl;
        closesocket(server_socket);
        WSACleanup();
#else
        close(server_socket);
#endif
        exit(-1);
    }

    if ((listen(server_socket, 8)) == -1) {
        perror("listen error: ");
        exit(-1);
    }

    struct sockaddr_in client;
    int client_socket;
    socklen_t len = sizeof(struct sockaddr_in);

    cout << "\n\t  ====== Welcome to the chat-room ======   " << endl << endl;

    while (1) {
        if ((client_socket = accept(server_socket, (struct sockaddr*)&client, &len)) == -1) {
            perror("accept error: ");
            exit(-1);
        }

        seed++;
        
        thread_data* data = new thread_data;
        data->client_socket = client_socket;
        data->id = seed;
        
        #ifdef _WIN32
        EnterCriticalSection(&clients_mtx);
        unsigned thread_id;
        HANDLE thread_handle = (HANDLE)_beginthreadex(NULL, 0, client_thread, (void*)data, 0, &thread_id);
        clients.push_back({seed, "Anonymous", client_socket, thread_handle});
        LeaveCriticalSection(&clients_mtx);
        #else
        pthread_t thread;
        pthread_create(&thread, NULL, client_thread, (void*)data);
        pthread_mutex_lock(&clients_mtx);
        clients.push_back({seed, "Anonymous", client_socket, thread});
        pthread_mutex_unlock(&clients_mtx);
        #endif
    }

    #ifdef _WIN32
    for (auto& client : clients) {
        WaitForSingleObject(client.thread_handle, INFINITE);
        CloseHandle(client.thread_handle);
    }
    
    DeleteCriticalSection(&cout_mtx);
    DeleteCriticalSection(&clients_mtx);
    #else
    for (auto& client : clients) {
        pthread_join(client.thread, NULL);
    }
    #endif

    close(server_socket);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}

#ifdef _WIN32
unsigned __stdcall client_thread(void* arg) {
    thread_data* data = (thread_data*)arg;
    handle_client(data->client_socket, data->id);
    delete data;
    return 0;
}
#else
void* client_thread(void* arg) {
    thread_data* data = (thread_data*)arg;
    handle_client(data->client_socket, data->id);
    delete data;
    return NULL;
}
#endif

string color(int code) {
    string colors[] = {"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m"};
    return colors[code % NUM_COLORS];
}

// Set name of client
void set_name(int id, char name[]) {
    #ifdef _WIN32
    EnterCriticalSection(&clients_mtx);
    #else
    pthread_mutex_lock(&clients_mtx);
    #endif
    
    for (auto& client : clients) {
        if (client.id == id) {
            client.name = string(name);
            break;
        }
    }
    
    #ifdef _WIN32
    LeaveCriticalSection(&clients_mtx);
    #else
    pthread_mutex_unlock(&clients_mtx);
    #endif
}

// For synchronization of cout statements
void shared_print(string str, bool endLine) {
    #ifdef _WIN32
    EnterCriticalSection(&cout_mtx);
    #else
    pthread_mutex_lock(&cout_mtx);
    #endif
    
    cout << str;
    if (endLine)
        cout << endl;
    
    #ifdef _WIN32
    LeaveCriticalSection(&cout_mtx);
    #else
    pthread_mutex_unlock(&cout_mtx);
    #endif
}

// Broadcast message to all clients except the sender
int broadcast_message(string message, int sender_id) {
    #ifdef _WIN32
    EnterCriticalSection(&clients_mtx);
    #else
    pthread_mutex_lock(&clients_mtx);
    #endif
    
    for (auto& client : clients) {
        if (client.id != sender_id) {
            // Add null terminator to ensure proper string reception
            send(client.socket, message.c_str(), message.length() + 1, 0);
        }
    }
    
    #ifdef _WIN32
    LeaveCriticalSection(&clients_mtx);
    #else
    pthread_mutex_unlock(&clients_mtx);
    #endif
    
    return 0;
}

// Broadcast a number to all clients except the sender
int broadcast_message(int num, int sender_id) {
    #ifdef _WIN32
    EnterCriticalSection(&clients_mtx);
    #else
    pthread_mutex_lock(&clients_mtx);
    #endif
    
    for (auto& client : clients) {
        if (client.id != sender_id) {
            char num_str[10];
            snprintf(num_str, sizeof(num_str), "%d", num);
            // Add null terminator to ensure proper string reception
            send(client.socket, num_str, strlen(num_str) + 1, 0);
        }
    }
    
    #ifdef _WIN32
    LeaveCriticalSection(&clients_mtx);
    #else
    pthread_mutex_unlock(&clients_mtx);
    #endif
    
    return 0;
}

void end_connection(int id) {
    #ifdef _WIN32
    EnterCriticalSection(&clients_mtx);
    #else
    pthread_mutex_lock(&clients_mtx);
    #endif
    
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (it->id == id) {
            #ifdef _WIN32
            CloseHandle(it->thread_handle);
            #endif
            close(it->socket);
            clients.erase(it);
            break;
        }
    }
    
    #ifdef _WIN32
    LeaveCriticalSection(&clients_mtx);
    #else
    pthread_mutex_unlock(&clients_mtx);
    #endif
}

void handle_client(int client_socket, int id) {
    char name[MAX_LEN], str[MAX_LEN];
    recv(client_socket, name, sizeof(name), 0);
    set_name(id, name);

    // Display welcome message
    string welcome_message = string(name) + " has joined";
    shared_print(color(id) + welcome_message + "\033[0m");

    // Send a special message to indicate a new user joined
    char type_str[2] = "1"; // Type 1: New user joined
    
    // Broadcast to all clients in sequence
    broadcast_message(string(type_str), id);
    broadcast_message(string(name), id);
    broadcast_message(id, id);
    broadcast_message(string(welcome_message), id);

    while (1) {
        int bytes_received = recv(client_socket, str, sizeof(str), 0);
        if (bytes_received <= 0)
            return;

        if (strcmp(str, "#exit") == 0) {
            // Display leaving message
            string message = string(name) + " has left";
            shared_print(color(id) + message + "\033[0m");

            // Send a special message to indicate a user left
            type_str[0] = '2'; // Type 2: User left
            
            // Broadcast to all clients in sequence
            broadcast_message(string(type_str), id);
            broadcast_message(string(name), id);
            broadcast_message(id, id);
            broadcast_message(string(message), id);
            
            end_connection(id);
            return;
        }

        // Display message
        shared_print(color(id) + name + ": " + str + "\033[0m");

        // Send a special message to indicate a regular chat message
        type_str[0] = '0'; // Type 0: Regular message
        
        // Broadcast to all clients in sequence
        broadcast_message(string(type_str), id);
        broadcast_message(string(name), id);
        broadcast_message(id, id);
        broadcast_message(string(str), id);
    }
}
