#ifndef __SOCK_H__
#define __SOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

/* for windows builds */
#ifdef __MINGW32__
#include <winsock.h>

/* for linux and mac builds */
#else
typedef int SOCKET;
#define INVALID_SOCKET  -1
#endif

int ConnectSocket(const char *hostName, short port, SOCKET *pSocket);
int BindSocket(short port, SOCKET *pSocket);
void DisconnectSocket(SOCKET sock);
int SocketDataAvailableP(SOCKET sock, int timeout);
int SendSocketData(SOCKET sock, void *buf, int len);
int ReceiveSocketData(SOCKET sock, void *buf, int len);

#ifdef __cplusplus
}
#endif

#endif
