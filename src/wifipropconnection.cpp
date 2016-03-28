#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wifipropconnection.h"

#define CALIBRATE_DELAY 10

#define HTTP_PORT       80
#define TELNET_PORT     23
#define DISCOVER_PORT   2000

int resetPin = 12;
extern int verbose;

WiFiPropConnection::WiFiPropConnection()
    : m_ipaddr(NULL),
      m_socket(INVALID_SOCKET)
{
    m_loaderBaudRate = WIFI_LOADER_BAUD_RATE;
    m_fastLoaderBaudRate = WIFI_FAST_LOADER_BAUD_RATE;
    m_programBaudRate = WIFI_PROGRAM_BAUD_RATE;
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

    if (GetInternetAddress(m_ipaddr, HTTP_PORT, &m_httpAddr) != 0)
        return -1;

    if (GetInternetAddress(m_ipaddr, TELNET_PORT, &m_telnetAddr) != 0)
        return -1;

    return 0;
}

int WiFiPropConnection::close()
{
    if (!isOpen())
        return -1;

    return 0;
}

int WiFiPropConnection::connect()
{
    if (!m_ipaddr)
        return -1;

    if (m_socket != INVALID_SOCKET)
        return -1;
        
    if (ConnectSocket(&m_telnetAddr, &m_socket) != 0)
        return -1;

    return 0;
}

int WiFiPropConnection::disconnect()
{
    if (m_socket == INVALID_SOCKET)
        return -1;
        
    CloseSocket(m_socket);
    m_socket = INVALID_SOCKET;
    
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
    
    /* use the initial loader baud rate */
    if (setBaudRate(loaderBaudRate()) != 0) 
        return -1;
        
    hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
POST /propeller/load?reset-pin=%d&baud-rate=%d HTTP/1.1\r\n\
Content-Length: %d\r\n\
\r\n", resetPin, loaderBaudRate(), imageSize);

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

#define MAX_IF_ADDRS    20
#define NAME_TAG        "\"name\": \""

int WiFiPropConnection::findModules(bool check, WiFiInfoList &list, int count)
{
    uint8_t txBuf[1024]; // BUG: get rid of this magic number!
    uint8_t rxBuf[1024]; // BUG: get rid of this magic number!
    IFADDR ifaddrs[MAX_IF_ADDRS];
    uint32_t *txNext;
    int ifCnt, tries, txCnt, cnt, i;
    SOCKADDR_IN addr;
    SOCKET sock;
    
    /* get all of the network interface addresses */
    if ((ifCnt = GetInterfaceAddresses(ifaddrs, MAX_IF_ADDRS)) < 0) {
        printf("error: GetInterfaceAddresses failed\n");
        return -1;
    }
    
    /* create a broadcast socket */
    if (OpenBroadcastSocket(DISCOVER_PORT, &sock) != 0) {
        printf("error: OpenBroadcastSocket failed\n");
        return -1;
    }
        
    /* initialize the broadcast message to indicate no modules found yet */
    txNext = (uint32_t *)txBuf;
    *txNext++ = 0; // indicates that this is a request not a response
    txCnt = sizeof(uint32_t);

    /* make a number of attempts at discovering modules */
    for (tries = 4; --tries >= 0; ) {

        /* send the broadcast packet to all interfaces */
        for (i = 0; i < ifCnt; ++i) {
            SOCKADDR_IN bcastaddr;

            /* build a broadcast address */
            bcastaddr = ifaddrs[i].bcast;
            bcastaddr.sin_port = htons(DISCOVER_PORT);
    
            /* send the broadcast packet */
            if (SendSocketDataTo(sock, txBuf, txCnt, &bcastaddr) != txCnt) {
                perror("error: SendSocketDataTo failed");
                CloseSocket(sock);
                return -1;
            }
        }
    
        /* receive wifi module responses */
        while (SocketDataAvailableP(sock, 250)) {

            /* get the next response */
            if ((cnt = ReceiveSocketDataAndAddress(sock, rxBuf, sizeof(rxBuf) - 1, &addr)) < 0) {
                printf("error: ReceiveSocketData failed\n");
                CloseSocket(sock);
                return -3;
            }
            rxBuf[cnt] = '\0';
        
            /* only process replies */
            if (cnt >= sizeof(uint32_t) && *(uint32_t *)rxBuf != 0) {
                std::string addressStr(AddressToString(&addr));
                const char *name, *p, *p2;
                char nameBuffer[128];
                printf("Address: %08x\n", addr.sin_addr.s_addr);
            
                /* make sure we don't already have a response from this module */
                WiFiInfoList::iterator i = list.begin();
                bool skip = false;
                while (i != list.end()) {
                    if (i->address() == addressStr) {
                    printf("Skipping duplicate\n");
                        skip = true;
                        break;
                    }
                    ++i;
                }
                if (skip)
                    continue;
            
                /* add the module's ip address to the next broadcast message */
                if (txCnt < sizeof(txBuf)) {
                    *txNext++ = addr.sin_addr.s_addr;
                    txCnt += sizeof(uint32_t);
                }
            
                if (verbose)
                    printf("from %s got: %s", AddressToString(&addr), rxBuf);
                
                if (!(p = strstr((char *)rxBuf, NAME_TAG)))
                    name = "";
                else {
                    p += strlen(NAME_TAG);
                    if (!(p2 = strchr(p, '"'))) {
                        CloseSocket(sock);
                        return -1;
                    }
                    else if (p2 - p >= (int)sizeof(nameBuffer)) {
                        CloseSocket(sock);
                        return -1;
                    }
                    strncpy(nameBuffer, p, p2 - p);
                    nameBuffer[p2 - p] = '\0';
                    name = nameBuffer;
                }
            
                WiFiInfo info(name, addressStr);
                list.push_back(info);
            
                if (count > 0 && --count == 0) {
                    CloseSocket(sock);
                    return 0;
                }
            }
        }
    }
    
    /* close the socket */
    CloseSocket(sock);
    
    /* return successfully */
    return 0;
}

bool WiFiPropConnection::isOpen()
{
    return m_socket != INVALID_SOCKET;
}

int WiFiPropConnection::setName(const char *name)
{
    uint8_t buffer[1024];
    int hdrCnt, result;
    
    hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
POST /system/update?name=%s HTTP/1.1\r\n\
\r\n", name);

    if (sendRequest(buffer, hdrCnt, buffer, sizeof(buffer), &result) == -1) {
        printf("error: name update request failed\n");
        return -1;
    }
    else if (result != 204) {
        printf("error: name update returned %d\n", result);
        return -1;
    }

    return 0;
}

int WiFiPropConnection::generateResetSignal()
{
    uint8_t buffer[1024];
    int hdrCnt, result;
    
    hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
POST /propeller/reset?reset-pin=%d HTTP/1.1\r\n\
\r\n", resetPin);

    if (sendRequest(buffer, hdrCnt, buffer, sizeof(buffer), &result) == -1) {
        printf("error: reset request failed\n");
        return -1;
    }
    else if (result != 200) {
        printf("error: reset returned %d\n", result);
        return -1;
    }

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
    
    if (baudRate != m_baudRate) {

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
    }
    
    return 0;
}

int WiFiPropConnection::terminal(bool checkForExit, bool pstMode)
{
    if (!connect())
        return -1;
    SocketTerminal(m_socket, checkForExit, pstMode);
    disconnect();
    return 0;
}

int WiFiPropConnection::sendRequest(uint8_t *req, int reqSize, uint8_t *res, int resMax, int *pResult)
{
    SOCKET sock;
    char buf[80];
    int cnt;
    
    if (ConnectSocket(&m_httpAddr, &sock) != 0) {
        printf("error: connect failed\n");
        return -1;
    }
    
    if (verbose) {
        printf("REQ: %d\n", reqSize);
        dumpHdr(req, reqSize);
    }
    
    if (SendSocketData(sock, req, reqSize) != reqSize) {
        printf("error: send request failed\n");
        CloseSocket(sock);
        return -1;
    }
    
    cnt = ReceiveSocketDataTimeout(sock, res, resMax, 10000);
    CloseSocket(sock);

    if (cnt == -1) {
        printf("error: receive response failed\n");
        return -1;
    }
    
    if (verbose) {
        printf("RES: %d\n", cnt);
        dumpResponse(res, cnt);
    }
    
    if (sscanf((char *)res, "%s %d", buf, pResult) != 2)
        return -1;
        
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

