#include <string.h>
#include <ctype.h>

#ifdef __MINGW32__
#else
#include <ifaddrs.h>
#endif

#include "sock.h"

#ifdef __MINGW32__

static int socketsInitialized = FALSE;

static int InitWinSock(void)
{
    WSADATA WsaData;

    /* initialize winsock */
    if (!socketsInitialized) {
        if (WSAStartup(0x0101, &WsaData) != 0)
            return -1;
        socketsInitialized = TRUE;
    }
    
    return 0;
}

#endif

/* GetInternetAddress - get an internet address */
int GetInternetAddress(const char *hostName, short port, SOCKADDR_IN *addr)
{
    HOSTENT *hostEntry;

#ifdef __MINGW32__
    if (InitWinSock() != 0)
        return -1;
#endif

    /* setup the address */
    memset(addr, 0, sizeof(SOCKADDR_IN));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);

    /* get the host address by address in dot notation */
    if (isdigit(hostName[0])) {
        unsigned long hostAddr = inet_addr(hostName);
        addr->sin_addr = *(IN_ADDR *)&hostAddr;
    }
    
    /* get the host address by name */
    else {
        if ((hostEntry = gethostbyname(hostName)) == NULL)
            return -1;
        addr->sin_addr = *(IN_ADDR *)*hostEntry->h_addr_list;
    }
    
    /* return successfully */
    return 0;
}

/* OpenBroadcastSocket - open a broadcast socket */
int OpenBroadcastSocket(SOCKET *pSocket)
{
    int broadcast = 1;
    SOCKET sock;
    
#ifdef __MINGW32__
    if (InitWinSock() != 0)
        return INVALID_SOCKET;
#endif

    /* create the socket */
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        return -1;

    /* enable broadcasting */
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *)&broadcast, sizeof(broadcast)) != 0) {
        CloseSocket(sock);
        return -1;
    }

    /* return the socket */
    *pSocket = sock;
    return 0;
}

/* ConnectSocket - connect to the server */
int ConnectSocket(uint32_t ipaddr, short port, SOCKET *pSocket)
{
    SOCKADDR_IN addr;
    SOCKET sock;
    
#ifdef __MINGW32__
    if (InitWinSock() != 0)
        return INVALID_SOCKET;
#endif

    /* create the socket */
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        return -1;

    /* setup the address */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(ipaddr);
        
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
    if (InitWinSock() != 0)
        return INVALID_SOCKET;
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

/* CloseSocket - close a socket */
void CloseSocket(SOCKET sock)
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
                    break;
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

/* SendSocketDataTo - send socket data to a specified address */
int SendSocketDataTo(SOCKET sock, void *buf, int len, SOCKADDR_IN *addr)
{
    return sendto(sock, buf, len, 0, (SOCKADDR *)addr, sizeof(SOCKADDR));
}

/* GetInterfaceAddresses - get the addresses of all IPv4 interfaces */
int GetInterfaceAddresses(IFADDR *addrs, int max)
{
#ifdef __MINGW32__
    return 0;
#else
    struct ifaddrs *list, *entry;
    int cnt;
    
    if (getifaddrs(&list) != 0)
        return -1;

    cnt = 0;
    for (entry = list; cnt < max && entry != NULL; entry = entry->ifa_next) {
        if (entry->ifa_addr->sa_family == AF_INET && !(entry->ifa_flags & IFF_LOOPBACK)) {
            addrs->addr = *(SOCKADDR_IN *)entry->ifa_addr;
            addrs->mask = *(SOCKADDR_IN *)entry->ifa_netmask;
            addrs->bcast = *(SOCKADDR_IN *)entry->ifa_broadaddr;
            ++addrs;
            ++cnt;
        }
    }
    
    freeifaddrs(list);
    
    return cnt;
#endif
}

const char *GetAddressStringX(uint32_t addr)
{
    IN_ADDR inaddr;
    inaddr.s_addr = htonl(addr);
    return inet_ntoa(inaddr);
}
