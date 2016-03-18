#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wifipropconnection.h"

#define CALIBRATE_DELAY         10

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

int resetPin = 12;
int verbose = 1;

WiFiPropConnection::WiFiPropConnection()
    : m_ipaddr(0), m_readyToConnect(false), m_socket(INVALID_SOCKET), m_baudRate(WIFI_INITIAL_BAUD_RATE)
{
}

WiFiPropConnection::~WiFiPropConnection()
{
    if (m_ipaddr)
        free(m_ipaddr);
    disconnect();
}

int WiFiPropConnection::setAddress(const char *ipaddr)
{
    if (m_ipaddr)
        free(m_ipaddr);

    if (!(m_ipaddr = (char *)malloc(strlen(ipaddr) + 1)))
        return -1;
    strcpy(m_ipaddr, ipaddr);

    return 0;
}

int WiFiPropConnection::setPort(short port)
{
    if (!m_ipaddr)
        return -1;

    if (GetInternetAddress(m_ipaddr, port, &m_addr) != 0)
        return -1;

    m_readyToConnect = true;

    return 0;
}

int WiFiPropConnection::connect()
{
    if (!m_readyToConnect)
        return -1;

    if (ConnectSocket(&m_addr, &m_socket) != 0)
        return -1;

    m_readyToConnect = false;

    return 0;
}

int WiFiPropConnection::disconnect()
{
    if (m_socket == INVALID_SOCKET)
        return -1;
    CloseSocket(m_socket);
    m_socket = INVALID_SOCKET;
    m_readyToConnect = true;
    return 0;
}

int WiFiPropConnection::identify(int *pVersion)
{
    return 0;
}

int WiFiPropConnection::loadImage(const uint8_t *image, int imageSize, LoadType loadType)
{
    uint8_t buffer[1024], *packet;
    int hdrCnt, result;
    
    if (setPort(80) != 0)
        return -1;

    hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
POST /propeller/load?reset-pin=%d&baud-rate=%d HTTP/1.1\r\n\
Content-Length: %d\r\n\
\r\n", resetPin, initialBaudRate(), imageSize);

    if (!(packet = (uint8_t *)malloc(hdrCnt + imageSize)))
        return -1;

    memcpy(packet,  buffer, hdrCnt);
    memcpy(&packet[hdrCnt], image, imageSize);
    
    if (sendRequest(packet, hdrCnt + imageSize, buffer, sizeof(buffer), &result) == -1) {
        printf("error: load request failed\n");
        return -1;
    }
    else if (result != 200) {
        printf("error: load returned %d\n", result);
        return -1;
    }
    
    return 0;
}

#define DEF_DISCOVER_PORT   2000
#define MAX_IF_ADDRS        10

int discover1(IFADDR *ifaddr, WiFiInfoList &list, int timeout)
{
    uint8_t txBuf[1024]; // BUG: get rid of this magic number!
    uint8_t rxBuf[1024]; // BUG: get rid of this magic number!
    SOCKADDR_IN bcastaddr;
    SOCKADDR_IN addr;
    SOCKET sock;
    int cnt;
    
    /* create a broadcast socket */
    if (OpenBroadcastSocket(DEF_DISCOVER_PORT, &sock) != 0) {
        printf("error: OpenBroadcastSocket failed\n");
        return -2;
    }
        
    /* build a broadcast address */
    bcastaddr = ifaddr->bcast;
    bcastaddr.sin_port = htons(DEF_DISCOVER_PORT);
    
    /* send the broadcast packet */
    sprintf((char *)txBuf, "Me here! Ignore this message.\n");
    if ((cnt = SendSocketDataTo(sock, txBuf, strlen((char *)txBuf), &bcastaddr)) != (int)strlen((char *)txBuf)) {
        perror("error: SendSocketDataTo failed");
        CloseSocket(sock);
        return -1;
    }

    /* receive Xbee responses */
    while (SocketDataAvailableP(sock, timeout)) {

        /* get the next response */
        memset(rxBuf, 0, sizeof(rxBuf));
        if ((cnt = ReceiveSocketDataAndAddress(sock, rxBuf, sizeof(rxBuf) - 1, &addr)) < 0) {
            printf("error: ReceiveSocketData failed\n");
            CloseSocket(sock);
            return -3;
        }
        rxBuf[cnt] = '\0';
        
        if (!strstr((char *)rxBuf, "Me here!"))
            printf("from %s got: %s", AddressToString(&addr), rxBuf);
    }
    
    /* close the socket */
    CloseSocket(sock);
    
    /* return successfully */
    return 0;
}

int WiFiPropConnection::findModules(const char *prefix, bool check, WiFiInfoList &list)
{
    IFADDR ifaddrs[MAX_IF_ADDRS];
    int cnt, i;
    
    if ((cnt = GetInterfaceAddresses(ifaddrs, MAX_IF_ADDRS)) < 0)
        return -1;
    
    for (i = 0; i < cnt; ++i) {
        int ret;
        if ((ret = discover1(&ifaddrs[i], list, 2000)) < 0)
            return ret;
    }
    
    return 0;
}

bool WiFiPropConnection::isOpen()
{
    return m_socket != INVALID_SOCKET;
}

int WiFiPropConnection::generateResetSignal()
{
    if (!isOpen())
        return -1;
    return 0;
}

int WiFiPropConnection::sendData(const uint8_t *buf, int len)
{
    if (!isOpen())
        return -1;
    return SendSocketData(m_socket, buf, len);
}

int WiFiPropConnection::receiveDataTimeout(uint8_t *buf, int len, int timeout)
{
    if (!isOpen())
        return -1;
    return ReceiveSocketDataTimeout(m_socket, buf, len, timeout);
}

int WiFiPropConnection::receiveDataExactTimeout(uint8_t *buf, int len, int timeout)
{
    if (!isOpen())
        return -1;
    return ReceiveSocketDataExactTimeout(m_socket, buf, len, timeout);
}

int WiFiPropConnection::setBaudRate(int baudRate)
{
    uint8_t buffer[1024];
    int hdrCnt, result;

    hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
POST /propeller/set-baud-rate?baud-rate=%d HTTP/1.1\r\n\
\r\n", baudRate);

    if (sendRequest(buffer, hdrCnt, buffer, sizeof(buffer), &result) == -1) {
        printf("error: set-baud-rate request failed\n");
        return -1;
    }
    else if (result != 200) {
        printf("error: set-baud-rate returned %d\n", result);
        return -1;
    }
    
    m_baudRate = baudRate;
    return 0;
}

int WiFiPropConnection::terminal(bool checkForExit, bool pstMode)
{
    if (!setPort(23) || !connect())
        return -1;
    SocketTerminal(m_socket, checkForExit, pstMode);
    disconnect();
    return 0;
}

int WiFiPropConnection::sendRequest(uint8_t *req, int reqSize, uint8_t *res, int resMax, int *pResult)
{
    char buf[80];
    int cnt;
    
    if (connect() != 0) {
        printf("error: connect failed\n");
        return -1;
    }
    
    if (verbose) {
        printf("REQ: %d\n", reqSize);
        dumpHdr(req, reqSize);
    }
    
    if (sendData(req, reqSize) != reqSize) {
        printf("error: send request failed\n");
        disconnect();
        return -1;
    }
    
    if ((cnt = receiveDataTimeout(res, resMax, 10000)) == -1) {
        printf("error: receive response failed\n");
        disconnect();
        return -1;
    }
    
    if (verbose) {
        printf("RES: %d\n", cnt);
        dumpResponse(res, cnt);
    }
    
    if (sscanf((char *)res, "%s %d", buf, pResult) != 2) {
        disconnect();
        return -1;
    }
        
    disconnect();
    
    return cnt;
}
    
void WiFiPropConnection::dumpHdr(const uint8_t *buf, int size)
{
    int startOfLine = true;
    const uint8_t *p = buf;
    while (p < buf + size) {
        if (*p == '\r') {
            if (startOfLine)
                break;
            startOfLine = true;
            putchar('\n');
        }
        else if (*p != '\n') {
            startOfLine = false;
            putchar(*p);
        }
        ++p;
    }
    putchar('\n');
}

void WiFiPropConnection::dumpResponse(const uint8_t *buf, int size)
{
    int startOfLine = true;
    const uint8_t *p = buf;
    const uint8_t *save;
    int cnt;
    
    while (p < buf + size) {
        if (*p == '\r') {
            if (startOfLine) {
                ++p;
                if (*p == '\n')
                    ++p;
                break;
            }
            startOfLine = true;
            putchar('\n');
        }
        else if (*p != '\n') {
            startOfLine = false;
            putchar(*p);
        }
        ++p;
    }
    putchar('\n');
    
    save = p;
    while (p < buf + size) {
        if (*p == '\r')
            putchar('\n');
        else if (*p != '\n')
            putchar(*p);
        ++p;
    }

    p = save;
    cnt = 0;
    while (p < buf + size) {
        printf("%02x ", *p++);
        if ((++cnt % 16) == 0)
            putchar('\n');
    }
    if ((cnt % 16) != 0)
        putchar('\n');
}

