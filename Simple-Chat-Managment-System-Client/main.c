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




void sendMsg(SOCKET s, enum CommType code,unsigned char * msg) {
    
    unsigned char sendBuf[DEFAULT_BUFLEN];
    int len = 0;

    if (msg != NULL) {

        len = strnlen(msg, DEFAULT_BUFLEN - 2);
        strcpy_s(sendBuf + 1, DEFAULT_BUFLEN - 1, msg);

    }

    if (len > 0) len++; //making sure we send terminator NULL

    sendBuf[0] = code;
    int iResult = send(s, sendBuf, len + 1, 0);
    if (code > DISCON_CUTOFF) {
        closesocket(s);
        pthread_exit(PTHREAD_CANCELED);
    }

}
pthread_mutex_t printfMutex = PTHREAD_MUTEX_INITIALIZER;

int update;
int currentRow = 0;
int keyboardBuffLen = 0;
char keyboardBuff[DEFAULT_BUFLEN];

int STOPFLAG = 0;

void moveCurs(int row,int col) {
    
    printf("\033[%d;%dH",row,col);

}

void* deferredInput(void * sin) {
    SOCKET s = *(SOCKET*)sin;
    while (STOPFLAG == 0) {
        
        char c = getch();

        pthread_mutex_lock(&printfMutex);

        if (c != -1 && keyboardBuffLen < DEFAULT_BUFLEN-1) {

            switch (c) {
            case '\n': case '\r':

                if (keyboardBuffLen == 0) 
                    break;

                if (keyboardBuff[0] == '/') {

                    if (strncmp(keyboardBuff+1, "leave", DEFAULT_BUFLEN) == 0) {

                        sendMsg(s, CLIEND_DISCONNECTED_MSG, NULL);
                        closesocket(s);
                        STOPFLAG = 1;

                    };

                }

                moveCurs(currentRow,0);
                printf("current user > %s\n", keyboardBuff);
                currentRow++;

                sendMsg(s, BROAD_MSG, keyboardBuff);
                keyboardBuffLen = 0;
                keyboardBuff[0] = '\0';
            
            break;

            case 127: case 8: //backspace
                if (keyboardBuffLen > 0) {

                    keyboardBuff[--keyboardBuffLen] = 0;

                    printf("%c %c", c,c);

                }
            
            break;
            default:
                keyboardBuff[keyboardBuffLen++] = c;
                keyboardBuff[keyboardBuffLen] = '\0';
                printf("%c", c);
            }

        }

        pthread_mutex_unlock(&printfMutex);

    }

}

void chatHandler(SOCKET s, char* username) {

    pthread_t tid;
    pthread_create(&tid, NULL, deferredInput, &s);
    
    unsigned char inputBuf[DEFAULT_BUFLEN];
    do {
       
        int iRecv = recv(s, inputBuf, DEFAULT_BUFLEN, NULL);
        if (iRecv > 0) {
            enum CommType type = inputBuf[0];
            unsigned char* msg = inputBuf + 1;
            if (iRecv > 1) {

                switch (type) {

                case CLIENT_CONNECTED_MSG:

                    pthread_mutex_lock(&printfMutex);

                    moveCurs(currentRow, 0);
                    printf("[%s has joined the lobby]", inputBuf + 1);

                    for (int i = 0; i < keyboardBuffLen; i++)
                        printf(" ");

                    currentRow++;

                    moveCurs(currentRow, 0);

                    printf("%s", keyboardBuff);

                    pthread_mutex_unlock(&printfMutex);

                    break;
                case BROAD_MSG:
                {
                    int i = 0;
                    for (; i < DEFAULT_BUFLEN; i++) {
                        if (msg[i] == ',') {
                            msg[i] = '\0';
                            break;
                        }

                    }

                    pthread_mutex_lock(&printfMutex);

                    moveCurs(currentRow, 0);

                    printf("%s > %s\n", msg, msg + i + 1);

                    for (int i = 0; i < keyboardBuffLen; i++)
                        printf(" ");

                    currentRow++;

                    moveCurs(currentRow, 0);

                    printf("%s", keyboardBuff);

                    pthread_mutex_unlock(&printfMutex);

                }
                break;
                case CLIEND_DISCONNECTED_MSG:

                    pthread_mutex_lock(&printfMutex);

                    moveCurs(currentRow, 0);
                    printf("[%s has left the lobby]", inputBuf + 1);

                    for (int i = 0; i < keyboardBuffLen; i++)
                        printf(" ");

                    currentRow++;

                    moveCurs(currentRow, 0);

                    printf("%s", keyboardBuff);

                    pthread_mutex_unlock(&printfMutex);

                    break;

                }

            }

        }
        else if (iRecv == 0) {
            printf("You have been disconnected by the server\n");
            closesocket(s);
            return;
        }
        else if (iRecv < 0) {
            printf("[SERVER ERROR] ");
            printf("socket failed with error: %ld\n", WSAGetLastError());
            closesocket(s);
            return;
        }


    } while (STOPFLAG == 0);

};


void printMisuseError() {
    printf("Correct usage:\n");
    printf("get lobby list:     <hostname> list\n");
    printf("join lobby:         <hostname> join [code] [username]\n");
    printf("create lobby:       <hostname> create [code] [private/public] [maxClients]\n");
}

int __cdecl main(int argc, unsigned char** argv){

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO cbsi;
    GetConsoleScreenBufferInfo(hConsole, &cbsi);

    currentRow = cbsi.dwCursorPosition.Y+1;

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

            if (action == JOIN_LOBBY) {

                if (t == OK) {

                    printf("You have joined lobby %s\n", argv[3]);
                    currentRow++;
                    moveCurs(currentRow, 0);

                    chatHandler(ConnectSocket, argv[4]);
                    closesocket(ConnectSocket);
                    WSACleanup();
                    return;
                
                } else if (t == INVALID_CODE) {

                    printf("Lobby %s not found in server\n", argv[3]);
                    closesocket(ConnectSocket);
                    WSACleanup();
                    return;

                }
                else if (t == LOBBY_FULL) {
                    printf("Lobby %s is full\n", argv[3]);
                    closesocket(ConnectSocket);
                    WSACleanup();
                    return;
                }
                else {
                    printf("unspecified error %d",t);
                    closesocket(ConnectSocket);
                    WSACleanup();
                    return;
                }

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