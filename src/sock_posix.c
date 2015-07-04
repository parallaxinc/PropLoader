#include <string.h>
#include <ctype.h>
#include "sock.h"

/* for windows builds */
#ifdef __MINGW32__
#include <windows.h>
#include <winsock.h>

/* for linux and mac builds */
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#define IN_ADDR struct in_addr
#define SOCKADDR_IN struct sockaddr_in
#define SOCKADDR struct sockaddr
#define HOSTENT struct hostent
#define closesocket(x)  close(x)
#endif

#ifdef __MINGW32__
static int socketsInitialized = FALSE;
#endif

/* ConnectSocket - connect to the server */
int ConnectSocket(const char *hostName, short port, SOCKET *pSocket)
{
    HOSTENT *hostEntry;
    SOCKADDR_IN addr;
    SOCKET sock;
    
#ifdef __MINGW32__
    WSADATA WsaData;

    /* initialize winsock */
    if (!socketsInitialized) {
        if (WSAStartup(0x0101, &WsaData) != 0)
            return INVALID_SOCKET;
        socketsInitialized = TRUE;
    }
#endif

    /* create the socket */
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        return -1;

    /* setup the address */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    /* get the host address by address in dot notation */
    if (isdigit(hostName[0])) {
        unsigned long hostAddr = inet_addr(hostName);
        addr.sin_addr = *(IN_ADDR *)&hostAddr;
    }
    
    /* get the host address by name */
    else {
        if ((hostEntry = gethostbyname(hostName)) == NULL) {
            closesocket(sock);
            return -1;
        }
        addr.sin_addr = *(IN_ADDR *)*hostEntry->h_addr_list;
    }
    
    /* connect to the server */
    if (connect(sock, (SOCKADDR *)&addr, sizeof(addr)) != 0) {
        closesocket(sock);
        return -1;
    }

    /* return the socket */
    *pSocket = sock;
    return 0;
}

/* BindSocket - bind a socket to a port */
int BindSocket(short port, SOCKET *pSocket)
{
    SOCKADDR_IN addr;
    SOCKET sock;
    
#ifdef __MINGW32__
    WSADATA WsaData;

    /* initialize winsock */
    if (!socketsInitialized) {
        if (WSAStartup(0x0101, &WsaData) != 0)
            return INVALID_SOCKET;
        socketsInitialized = TRUE;
    }
#endif

    /* create the socket */
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        return -1;

    /* setup the address */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    /* bind the socket to the port */
    if (bind(sock, (SOCKADDR *)&addr, sizeof(addr)) != 0) {
        closesocket(sock);
        return -1;
    }

    /* return the socket */
    *pSocket = sock;
    return 0;
}

/* DisconnectSocket - close a connection */
void DisconnectSocket(SOCKET sock)
{
    struct timeval tv;
    fd_set sockets;
    char buf[512];

    tv.tv_sec = 0;
    tv.tv_usec = 1000;

    /* wait for the close to complete */
    for (;;) {
        FD_ZERO(&sockets);
        FD_SET(sock, &sockets);
        if (select(sock + 1, &sockets, NULL, NULL, &tv) > 0) {
            if (FD_ISSET(sock, &sockets))
                if (recv(sock, buf, sizeof(buf), 0) == 0)
                    break;;
        }
        else
            break;
    }

    /* close the socket */
    closesocket(sock);
}

/* SocketDataAvailableP - check for data being available on a socket */
int SocketDataAvailableP(SOCKET sock, int timeout)
{
    struct timeval timeVal;
    fd_set sockets;
    int cnt;

    /* setup the read socket set */
    FD_ZERO(&sockets);
    FD_SET(sock, &sockets);

    timeVal.tv_sec = timeout / 1000;
    timeVal.tv_usec = (timeout % 1000) * 1000;

    /* check for data available */
    cnt = select(sock + 1, &sockets, NULL, NULL, timeout < 0 ? NULL : &timeVal);

    /* return whether data is available */
    return cnt > 0 && FD_ISSET(sock, &sockets);
}

/* SendSocketData - send socket data */
int SendSocketData(SOCKET sock, void *buf, int len)
{
    return send(sock, buf, len, 0);
}

/* ReceiveSocketData - receive socket data */
int ReceiveSocketData(SOCKET sock, void *buf, int len)
{
    return recv(sock, buf, len, 0);
}
