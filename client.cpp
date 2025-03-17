#include <iostream>
#include <string>
#include <cstring>
#include <vector>
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

bool exit_flag=false;
#ifdef _WIN32
HANDLE t_send_handle, t_recv_handle;
#else
pthread_t t_send, t_recv;
#endif
int client_socket;
string def_col="\033[0m";
string colors[]={"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m"};

void catch_ctrl_c(int signal);
string color(int code);
int eraseText(int cnt);
#ifdef _WIN32
unsigned __stdcall send_message_thread(void* arg);
unsigned __stdcall recv_message_thread(void* arg);
#else
void* send_message_thread(void* arg);
void* recv_message_thread(void* arg);
#endif
void send_message(int client_socket);
void recv_message(int client_socket);

int main()
{
#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed" << endl;
        return -1;
    }
#endif

    if((client_socket=socket(AF_INET,SOCK_STREAM,0))==-1)
    {
        perror("socket: ");
        exit(-1);
    }

    struct sockaddr_in client;
    client.sin_family=AF_INET;
    client.sin_port=htons(8888); // Changed port to 8888 to match server
    client.sin_addr.s_addr=inet_addr("127.0.0.1"); // Connect to localhost
    memset(&client.sin_zero, '\0', sizeof(client.sin_zero));

    if((connect(client_socket,(struct sockaddr *)&client,sizeof(struct sockaddr_in)))==-1)
    {
        perror("connect: ");
#ifdef _WIN32
        closesocket(client_socket);
        WSACleanup();
#else
        close(client_socket);
#endif
        exit(-1);
    }
    signal(SIGINT, catch_ctrl_c);
    char name[MAX_LEN];
    cout<<"Enter your name : ";
    cin.getline(name,MAX_LEN);
    send(client_socket,name,sizeof(name),0);

    cout<<colors[NUM_COLORS-1]<<"\n\t  ====== Welcome to the chat-room ======   "<<endl<<def_col;

    #ifdef _WIN32
    unsigned thread_id;
    t_send_handle = (HANDLE)_beginthreadex(NULL, 0, send_message_thread, (void*)client_socket, 0, &thread_id);
    t_recv_handle = (HANDLE)_beginthreadex(NULL, 0, recv_message_thread, (void*)client_socket, 0, &thread_id);
    
    WaitForSingleObject(t_send_handle, INFINITE);
    WaitForSingleObject(t_recv_handle, INFINITE);
    
    CloseHandle(t_send_handle);
    CloseHandle(t_recv_handle);
    #else
    pthread_create(&t_send, NULL, send_message_thread, (void*)&client_socket);
    pthread_create(&t_recv, NULL, recv_message_thread, (void*)&client_socket);
    
    pthread_join(t_send, NULL);
    pthread_join(t_recv, NULL);
    #endif
    
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}

#ifdef _WIN32
unsigned __stdcall send_message_thread(void* arg) {
    int client_socket = (int)arg;
    send_message(client_socket);
    return 0;
}

unsigned __stdcall recv_message_thread(void* arg) {
    int client_socket = (int)arg;
    recv_message(client_socket);
    return 0;
}
#else
void* send_message_thread(void* arg) {
    int client_socket = *((int*)arg);
    send_message(client_socket);
    return NULL;
}

void* recv_message_thread(void* arg) {
    int client_socket = *((int*)arg);
    recv_message(client_socket);
    return NULL;
}
#endif

// Handler for "Ctrl + C"
void catch_ctrl_c(int signal) 
{
    char str[MAX_LEN]="#exit";
    send(client_socket,str,sizeof(str),0);
    exit_flag=true;
    
    #ifdef _WIN32
    TerminateThread(t_send_handle, 0);
    TerminateThread(t_recv_handle, 0);
    #else
    pthread_cancel(t_send);
    pthread_cancel(t_recv);
    #endif
    
    close(client_socket);
#ifdef _WIN32
    WSACleanup();
#endif
    exit(signal);
}

string color(int code)
{
	return colors[code%NUM_COLORS];
}

// Erase text from terminal
int eraseText(int cnt)
{
	char back_space=8;
	for(int i=0; i<cnt; i++)
	{
		cout<<back_space;
	}
	return cnt; // Return a value to fix the warning
}

// Send message to everyone
void send_message(int client_socket)
{
	while(1)
	{
		cout<<colors[1]<<"You : "<<def_col;
		char str[MAX_LEN];
		cin.getline(str,MAX_LEN);
		send(client_socket,str,sizeof(str),0);
		if(strcmp(str,"#exit")==0)
		{
			exit_flag=true;
			#ifdef _WIN32
            TerminateThread(t_recv_handle, 0);
            #else
            pthread_cancel(t_recv);
            #endif
			close(client_socket);
			return;
		}	
	}		
}

// Receive message
void recv_message(int client_socket)
{
	while(1)
	{
		if(exit_flag)
			return;
			
		// First receive the message type
		char type_str[2];
		int bytes_received = recv(client_socket, type_str, sizeof(type_str), 0);
		if(bytes_received <= 0)
			continue;
			
		int msg_type = type_str[0] - '0';  // Convert char to int
		
		// Receive the sender's name
		char name[MAX_LEN];
		bytes_received = recv(client_socket, name, sizeof(name), 0);
		if(bytes_received <= 0)
			continue;
			
		// Receive the color code (sender's ID)
		char color_str[10];
		bytes_received = recv(client_socket, color_str, sizeof(color_str), 0);
		if(bytes_received <= 0)
			continue;
		int color_code = atoi(color_str);
		
		// Receive the actual message
		char str[MAX_LEN];
		bytes_received = recv(client_socket, str, sizeof(str), 0);
		if(bytes_received <= 0)
			continue;
			
		// Erase the current line
		eraseText(6);
		
		// Display the message based on its type
		switch(msg_type) {
			case 0:  // Regular message
				cout << color(color_code) << name << " : " << def_col << str << endl;
				break;
			case 1:  // New user joined
			case 2:  // User left
				cout << color(color_code) << str << def_col << endl;
				break;
			default:
				cout << color(color_code) << str << def_col << endl;
		}
		
		// Redisplay the input prompt
		cout << colors[1] << "You : " << def_col;
		fflush(stdout);
	}	
}
