#ifndef __SOCK_H__
#define __SOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

/* for windows builds */
#ifdef __MINGW32__
#include <stdint.h>
#include <windows.h>
#include <winsock2.h>
typedef unsigned long in_addr_t;

/* for linux and mac builds */
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#define IN_ADDR struct in_addr
#define SOCKADDR_IN struct sockaddr_in
#define SOCKADDR struct sockaddr
#define HOSTENT struct hostent
#define closesocket(x)  close(x)
typedef int SOCKET;
#define INVALID_SOCKET  -1
#endif

typedef struct {
    SOCKADDR_IN addr;
    SOCKADDR_IN mask;
    SOCKADDR_IN bcast;
} IFADDR;

int GetInterfaceAddresses(IFADDR *addrs, int max);
int GetInternetAddress(const char *hostName, short port, SOCKADDR_IN *addr);
const char *AddrToString(uint32_t addr);
int StringToAddr(const char *addr, uint32_t *pAddr);
int OpenBroadcastSocket(short port, SOCKET *pSocket);
int ConnectSocket(SOCKADDR_IN *addr, SOCKET *pSocket);
int ConnectSocketTimeout(SOCKADDR_IN *addr, int timeout, SOCKET *pSocket);
int BindSocket(short port, SOCKET *pSocket);
void CloseSocket(SOCKET sock);
int SocketDataAvailableP(SOCKET sock, int timeout);
int SendSocketData(SOCKET sock, const void *buf, int len);
int ReceiveSocketData(SOCKET sock, void *buf, int len);
int ReceiveSocketDataTimeout(SOCKET sock, void *buf, int len, int timeout);
int ReceiveSocketDataExactTimeout(SOCKET sock, void *buf, int len, int timeout);
int SendSocketDataTo(SOCKET sock, const void *buf, int len, SOCKADDR_IN *addr);
int ReceiveSocketDataFrom(SOCKET sock, void *buf, int len, SOCKADDR_IN *addr);
int ReceiveSocketDataAndAddress(SOCKET sock, void *buf, int len, SOCKADDR_IN *addr);
const char *AddressToString(SOCKADDR_IN *addr);
void SocketTerminal(SOCKET sock, int check_for_exit, int pst_mode);

#ifdef __cplusplus
}
#endif

#endif
