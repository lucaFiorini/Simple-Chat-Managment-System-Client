#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <pthread.h>

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_CODELEN 8
#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"
#define MAX_ARGS 20
#define USERNAME_LEN 35

#define SEPARATOR ,

enum CommType {

    /* DATA / INFO: */

    OK = 0,
    GENERAL_SERVICE_MSG = 1,
    CLIENT_CONNECTED_MSG = 2,
    CLIEND_DISCONNECTED_MSG = 3,

    BROAD_MSG = 20,
    PRIV_USER_MSG = 21,

    PING = 40,

    /*Main menu options*/

    CREATE_LOBBY = 50,
    JOIN_LOBBY = 51,
    LOBBY_LIST = 52,

    /* Errors :  */

    DISCON_CUTOFF = 99,

    INCORRECT_ARGS = 100,
    CODE_TOO_LONG = 101,

    CODE_TAKEN = 120,
    SERVER_FULL = 121,
    TOO_FEW_CLIENTS = 122,

    LOBBY_FULL = 150,
    INVALID_CODE = 151,

    LOBBY_CLOSED = 200,
    KICKED = 201,

    OTHER = 255,
};




void sendMsg(SOCKET *s, enum CommType req,unsigned char * msg) {
    
}

int update;
char keyboardBuff[DEFAULT_BUFLEN];
void* deferredInput() {

    while (1) {
        char c = getch();


    }

}
void chatHandler(SOCKET s, char* username) {

    long len;
    ioctlsocket(s, FIONREAD, &len);
    pthread_t tid;
    pthread_create(&tid, NULL, deferredInput, NULL);
    
    char inputBuf[DEFAULT_BUFLEN];
    do {
        Sleep(100);
        int iRecv = recv(s, inputBuf, DEFAULT_BUFLEN, NULL);
        
        if (iRecv > 0) {

            printf("%s\n", inputBuf + 1);

        }


    } while (1);

};


void printMisuseError() {
    printf("Correct usage:\n");
    printf("get lobby list:     <hostname> list\n");
    printf("join lobby:         <hostname> join [code] [username]\n");
    printf("create lobby:       <hostname> create [code] [private/public] [maxClients]\n");
}

int __cdecl main(int argc, unsigned char** argv){

    if (argc < 3) {
        printMisuseError();
        return 1;
    }

    enum CommType action;

    unsigned char* command = argv[2];
    unsigned char sendbuf[DEFAULT_BUFLEN] = {'\0'};


    unsigned char* argsStr = sendbuf + 1;
    for (int i = 3; i < argc; i++) {

        strcat_s(argsStr, DEFAULT_BUFLEN-1, argv[i]);
        strcat_s(argsStr, DEFAULT_BUFLEN-1, ",");

    }
 

    argsStr[strlen(argsStr) - 1] = '\0';


    if (strcmp(command, "create") == 0) {

        action = CREATE_LOBBY;

        if (argc != 6) {
            printMisuseError();
            return 1;
        }

    }

    else if (strcmp(command, "join") == 0) {

        action = JOIN_LOBBY;

        if (argc != 5) {
            printMisuseError();
            return 1;
        }

    }

    else if (strcmp(command, "list") == 0)
        action = LOBBY_LIST;

    else {
        printMisuseError();
        return 1;
    }

    sendbuf[0] = (unsigned char)action;


    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;

    unsigned char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;
    

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(argv[1], DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        
        if (ConnectSocket == INVALID_SOCKET) {
            
            printf("socket failed with error: %ld\n", WSAGetLastError());
            return 1;

        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        
        break;

    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {

        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;

    }


    

    // Send an initial buffer
    iResult = send(ConnectSocket, sendbuf, (int)strlen(argsStr)+2, NULL);

    if (iResult == SOCKET_ERROR) {

        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    
    }

    //printf("Bytes Sent: %ld\n", iResult);

    // shutdown the connection since no more data will be sent
    iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {

        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    
    }

    if (action == LOBBY_LIST)
        printf("Lobby code \t Connected Clients\n");

    do {

        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        
        enum CommType t = recvbuf[0];
        unsigned char* msg = recvbuf + 1;

        if (iResult > 0) {

            if (t > DISCON_CUTOFF) {
                printf("Error %d\n", t);
                iResult = 0;
            }

            if (iResult > 1)
                printf("%s\n", msg);

            if (t == OK && action == JOIN_LOBBY) {

                printf("You have joined lobby %s\n", argv[3]);
                chatHandler(ConnectSocket, argv[4]);
                closesocket(ConnectSocket);
                WSACleanup();
                return;

            }

            if (t == OK && action == CREATE_LOBBY)
                printf("Lobby %s created succsessfully\n", argv[3]);

        }

    } while (iResult > 0);

    // cleanup
    closesocket(ConnectSocket);
    WSACleanup();

    return 0;
}