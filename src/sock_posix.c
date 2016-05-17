#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>

#ifdef __MINGW32__
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <conio.h>
#else
#include <ifaddrs.h>
#include <termios.h>
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
int OpenBroadcastSocket(short port, SOCKET *pSocket)
{
    int broadcast = 1;
    SOCKADDR_IN addr;
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

    /* setup the address */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
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

/* ConnectSocket - connect to a server */
int ConnectSocket(SOCKADDR_IN *addr, SOCKET *pSocket)
{
    SOCKET sock;
    
#ifdef __MINGW32__
    if (InitWinSock() != 0)
        return INVALID_SOCKET;
#endif

    /* create the socket */
    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        return -1;

    /* connect to the server */
    if (connect(sock, (SOCKADDR *)addr, sizeof(*addr)) != 0) {
        closesocket(sock);
        return -1;
    }

    /* return the socket */
    *pSocket = sock;
    return 0;
}

/* ConnectSocketTimeout - connect to a server with a timeout */
int ConnectSocketTimeout(SOCKADDR_IN *addr, int timeout, SOCKET *pSocket)
{
#ifdef __MINGW32__
    return ConnectSocket(addr, pSocket);
#else
    int flags, err;
    SOCKET sock;
    
#ifdef __MINGW32__
    if (InitWinSock() != 0)
        return INVALID_SOCKET;
#endif

    /* create the socket */
    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        return -1;

    /* set the socket to non-blocking mode */
    flags = fcntl(sock, F_GETFL, 0);
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) != 0) {
        closesocket(sock);
        return -1;
    }

    /* connect to the server */
    if (connect(sock, (SOCKADDR *)addr, sizeof(*addr)) != 0) {
        struct timeval timeVal;
        socklen_t optLen;
        fd_set sockets;
            
        /* fail on any error other than "in progress" */
        if (errno != EINPROGRESS) {
            closesocket(sock);
            return -1;
        }
        
        /* setup the read socket set */
        FD_ZERO(&sockets);
        FD_SET(sock, &sockets);

        /* setup the timeout */
        timeVal.tv_sec = timeout / 1000;
        timeVal.tv_usec = (timeout % 1000) * 1000;

        /* wait for the connect to complete or a timeout */
        if (select(sock + 1, NULL, &sockets, NULL, &timeVal) <= 0 || !FD_ISSET(sock, &sockets)) {
            closesocket(sock);
            return -1;
        }

        /* make sure the connection was successful */
        optLen = sizeof(err);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &optLen) != 0 || err != 0) {
            closesocket(sock);
            return -1;
        }    
    }
    
    /* set the socket back to blocking mode */
    if (fcntl(sock, F_SETFL, flags) != 0) {
        closesocket(sock);
        return -1;
    }

    /* return the socket */
    *pSocket = sock;
    return 0;
#endif
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
    addr.sin_addr.s_addr = INADDR_ANY;
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
int SendSocketData(SOCKET sock, const void *buf, int len)
{
    return send(sock, buf, len, 0);
}

/* SendSocketDataTo - send socket data to a specified address */
int SendSocketDataTo(SOCKET sock, const void *buf, int len, SOCKADDR_IN *addr)
{
    return sendto(sock, buf, len, 0, (SOCKADDR *)addr, sizeof(SOCKADDR));
}

/* ReceiveSocketData - receive socket data */
int ReceiveSocketData(SOCKET sock, void *buf, int len)
{
    return recv(sock, buf, len, 0);
}

/* ReceiveSocketDataTimeout - receive socket data */
int ReceiveSocketDataTimeout(SOCKET sock, void *buf, int len, int timeout)
{
    struct timeval toval;
    fd_set set;

    FD_ZERO(&set);
    FD_SET(sock, &set);

    toval.tv_sec = timeout / 1000;
    toval.tv_usec = (timeout % 1000) * 1000;

    if (select(sock + 1, &set, NULL, NULL, &toval) > 0) {
        if (FD_ISSET(sock, &set)) {
            int bytes = (int)recv(sock, buf, len, 0);
            return bytes;
        }
    }

    return -1;
}

/* ReceiveSocketDataExactTimeout - receive an exact amount of socket data */
int ReceiveSocketDataExactTimeout(SOCKET sock, void *buf, int len, int timeout)
{
    return ReceiveSocketDataTimeout(sock, buf, len, timeout);
}

/* ReceiveSocketDataAndAddress - receive socket data and sender's address */
int ReceiveSocketDataAndAddress(SOCKET sock, void *buf, int len, SOCKADDR_IN *addr)
{
    SOCKADDR x;
#ifdef __MINGW32__
    int xlen = sizeof(x);
#else
    unsigned int xlen = sizeof(x);
#endif
    int cnt;
    if ((cnt = recvfrom(sock, buf, len, 0, &x, &xlen)) < 0 || xlen != sizeof(SOCKADDR_IN))
        return cnt;
    *addr = *(SOCKADDR_IN *)&x;
    return cnt;
}

const char *AddressToString(SOCKADDR_IN *addr)
{
    return inet_ntoa(addr->sin_addr);
}

/* escape from terminal mode */
#define ESC         0x1b

/*
 * if "check_for_exit" is true, then
 * a sequence EXIT_CHAR 00 nn indicates that we should exit
 */
#define EXIT_CHAR   0xff

void SocketTerminal(SOCKET sock, int check_for_exit, int pst_mode)
{
#ifdef __MINGW32__
    int sawexit_char = 0;
    int sawexit_valid = 0;
    int exitcode = 0;
    int continue_terminal = 1;

    while (continue_terminal) {
        uint8_t buf[1];
        if (ReceiveSocketDataTimeout(sock, buf, 1, 0) != -1) {
            if (sawexit_valid) {
                exitcode = buf[0];
                continue_terminal = 0;
            }
            else if (sawexit_char) {
                if (buf[0] == 0) {
                    sawexit_valid = 1;
                } else {
                    putchar(EXIT_CHAR);
                    putchar(buf[0]);
                    fflush(stdout);
                }
            }
            else if (check_for_exit && buf[0] == EXIT_CHAR) {
                sawexit_char = 1;
            }
            else {
                putchar(buf[0]);
                if (pst_mode && buf[0] == '\r')
                    putchar('\n');
                fflush(stdout);
            }
        }
        else if (kbhit()) {
            if ((buf[0] = getch()) == ESC)
                break;
            SendSocketData(sock, buf, 1);
        }
    }

    if (check_for_exit && sawexit_valid) {
        exit(exitcode);
    }
#else
    struct termios oldt, newt;
    char buf[128], realbuf[256]; // double in case buf is filled with \r in PST mode
    ssize_t cnt;
    fd_set set;
    int exit_char = 0xdead; /* not a valid character */
    int sawexit_char = 0;
    int sawexit_valid = 0; 
    int exitcode = 0;
    int continue_terminal = 1;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO | ISIG);
    newt.c_iflag &= ~(ICRNL | INLCR);
    newt.c_oflag &= ~OPOST;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    if (check_for_exit)
        exit_char = EXIT_CHAR;

#if 0
    /* make it possible to detect breaks */
    tcgetattr(sock, &newt);
    newt.c_iflag &= ~IGNBRK;
    newt.c_iflag |= PARMRK;
    tcsetattr(sock, TCSANOW, &newt);
#endif

    do {
        FD_ZERO(&set);
        FD_SET(sock, &set);
        FD_SET(STDIN_FILENO, &set);
        if (select(sock + 1, &set, NULL, NULL, NULL) > 0) {
            if (FD_ISSET(sock, &set)) {
                if ((cnt = recv(sock, buf, sizeof(buf), 0)) > 0) {
                    int i;
                    // check for breaks
                    ssize_t realbytes = 0;
                    for (i = 0; i < cnt; i++) {
                      if (sawexit_valid)
                        {
                          exitcode = buf[i];
                          continue_terminal = 0;
                        }
                      else if (sawexit_char) {
                        if (buf[i] == 0) {
                          sawexit_valid = 1;
                        } else {
                          realbuf[realbytes++] = exit_char;
                          realbuf[realbytes++] = buf[i];
                          sawexit_char = 0;
                        }
                      } else if (((int)buf[i] & 0xff) == exit_char) {
                        sawexit_char = 1;
                      } else {
                        realbuf[realbytes++] = buf[i];
                        if (pst_mode && buf[i] == '\r')
                            realbuf[realbytes++] = '\n';
                      }
                    }
                    write(fileno(stdout), realbuf, realbytes);
                }
            }
            if (FD_ISSET(STDIN_FILENO, &set)) {
                if ((cnt = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
                    int i;
                    for (i = 0; i < cnt; ++i) {
                        if (buf[i] == ESC)
                            goto done;
                    }
                    write(sock, buf, cnt);
                }
            }
        }
    } while (continue_terminal);

done:
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    if (sawexit_valid)
        exit(exitcode);
#endif
}

/* GetInterfaceAddresses - get the addresses of all IPv4 interfaces */
int GetInterfaceAddresses(IFADDR *addrs, int max)
{
#ifdef __MINGW32__
    // adapted from an example in the Microsoft documentation
    PMIB_IPADDRTABLE pIPAddrTable;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;
    int cnt, i;

    // Before calling AddIPAddress we use GetIpAddrTable to get
    // an adapter to which we can add the IP.
    pIPAddrTable = (MIB_IPADDRTABLE *)malloc(sizeof (MIB_IPADDRTABLE));
    if (!pIPAddrTable)
        return -1;
        
    // Make an initial call to GetIpAddrTable to get the
    // necessary size into the dwSize variable
    if (GetIpAddrTable(pIPAddrTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER) {
        free(pIPAddrTable);
        pIPAddrTable = (MIB_IPADDRTABLE *)malloc(dwSize);
        if (!pIPAddrTable)
            return -1;
    }
    
    // Make a second call to GetIpAddrTable to get the actual data we want
    if ((dwRetVal = GetIpAddrTable( pIPAddrTable, &dwSize, 0 )) != NO_ERROR) 
        return -1;
    
    cnt = 0;
    for (i = 0; cnt < max && i < (int)pIPAddrTable->dwNumEntries; ++i) {
        addrs->addr.sin_family = AF_INET;
        addrs->addr.sin_addr.s_addr = (uint32_t)pIPAddrTable->table[i].dwAddr;
        addrs->mask.sin_family = AF_INET;
        addrs->mask.sin_addr.s_addr = (uint32_t)pIPAddrTable->table[i].dwMask;
        addrs->bcast.sin_family = AF_INET;
        addrs->bcast.sin_addr.s_addr = addrs->addr.sin_addr.s_addr | ~addrs->mask.sin_addr.s_addr;
        ++addrs;
        ++cnt;
    }

    free(pIPAddrTable);

    return cnt;
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

const char *AddrToString(uint32_t addr)
{
    IN_ADDR inaddr;
    inaddr.s_addr = addr;
    return inet_ntoa(inaddr);
}

int StringToAddr(const char *addr, uint32_t *pAddr)
{
    in_addr_t inaddr;
    if ((inaddr = inet_addr(addr)) == (in_addr_t)(-1))
        return -1;
    *pAddr = inaddr;
    return 0;
}

