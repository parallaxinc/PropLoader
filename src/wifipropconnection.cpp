#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wifipropconnection.h"

#define CALIBRATE_DELAY 10

#define HTTP_PORT       80
#define TELNET_PORT     23
#define DISCOVER_PORT   32420

int resetPin = 12;
extern int verbose;

WiFiPropConnection::WiFiPropConnection()
    : m_ipaddr(NULL),
      m_version(NULL),
      m_telnetSocket(INVALID_SOCKET)
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

int WiFiPropConnection::checkVersion()
{
    if (getVersion() != 0)
        return -1;
    return strncmp(WIFI_REQUIRED_MAJOR_VERSION, m_version, strlen(WIFI_REQUIRED_MAJOR_VERSION)) == 0 ? 0 : -1;
}

int WiFiPropConnection::close()
{
    if (!isOpen())
        return -1;
        
    disconnect();

    return 0;
}

int WiFiPropConnection::connect()
{
    if (isOpen())
        return -1;
        
    if (!m_ipaddr)
        return -1;

    if (ConnectSocketTimeout(&m_telnetAddr, CONNECT_TIMEOUT, &m_telnetSocket) != 0)
        return -1;

    return 0;
}

int WiFiPropConnection::disconnect()
{
    if (m_telnetSocket == INVALID_SOCKET)
        return -1;
        
    CloseSocket(m_telnetSocket);
    m_telnetSocket = INVALID_SOCKET;
    
    return 0;
}

int WiFiPropConnection::identify(int *pVersion)
{
    return 0;
}

int WiFiPropConnection::loadImage(const uint8_t *image, int imageSize, uint8_t *response, int responseSize)
{
    uint8_t buffer[1024], *packet, *p;
    int hdrCnt, result, cnt;
    
    /* use the initial loader baud rate */
    if (setBaudRate(loaderBaudRate()) != 0) 
        return -1;
        
    hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
POST /propeller/load?reset-pin=%d&baud-rate=%d&response-size=%d&response-timeout=1000 HTTP/1.1\r\n\
Content-Length: %d\r\n\
\r\n", resetPin, loaderBaudRate(), responseSize, imageSize);

    if (!(packet = (uint8_t *)malloc(hdrCnt + imageSize)))
        return -1;

    memcpy(packet,  buffer, hdrCnt);
    memcpy(&packet[hdrCnt], image, imageSize);
    
    if ((cnt = sendRequest(packet, hdrCnt + imageSize, buffer, sizeof(buffer), &result)) == -1) {
        printf("error: load request failed\n");
        return -1;
    }
    else if (result != 200) {
        printf("error: load returned %d\n", result);
        return -1;
    }
    
    /* find the response body */
    p = buffer;
    while (cnt >= 4 && (p[0] != '\r' || p[1] != '\n' || p[2] != '\r' || p[3] != '\n')) {
        --cnt;
        ++p;
    }
    
    /* make sure we found the \r\n\r\n that terminates the header */
    if (cnt < 4)
        return -1;
    cnt -= 4;
    p += 4;
    
    /* copy the body to the response if it fits */
    if (cnt > responseSize)
        return -1;
    memcpy(response, p, cnt);
        
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
#define MACADDR_TAG     "\"mac address\": \""

int WiFiPropConnection::findModules(bool show, WiFiInfoList &list, int count)
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

    /* keep trying until we have this many attempts that return no new modules */
    tries = DISCOVER_ATTEMPTS;
    
    /* make a number of attempts at discovering modules */
    while (tries > 0) {
        int numberFound = 0;

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
        while (SocketDataAvailableP(sock, DISCOVER_REPLY_TIMEOUT)) {

            /* get the next response */
            if ((cnt = ReceiveSocketDataAndAddress(sock, rxBuf, sizeof(rxBuf) - 1, &addr)) < 0) {
                printf("error: ReceiveSocketData failed\n");
                CloseSocket(sock);
                return -3;
            }
            rxBuf[cnt] = '\0';
        
            /* only process replies */
            if (cnt >= (int)sizeof(uint32_t) && *(uint32_t *)rxBuf != 0) {
                std::string addressStr(AddressToString(&addr));
                const char *name, *macAddr, *p, *p2;
                char nameBuffer[128];
                char macAddrBuffer[128];
                
                /* make sure we don't already have a response from this module */
                WiFiInfoList::iterator i = list.begin();
                bool skip = false;
                while (i != list.end()) {
                    if (i->address() == addressStr) {
                        if (verbose)
                            printf("Skipping duplicate: %s\n", AddressToString(&addr));
                        skip = true;
                        break;
                    }
                    ++i;
                }
                if (skip)
                    continue;
                    
                /* count this as a module found on this attempt */
                ++numberFound;
            
                /* add the module's ip address to the next broadcast message */
                if (txCnt < (int)sizeof(txBuf)) {
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
            
                if (!(p = strstr((char *)rxBuf, MACADDR_TAG)))
                    macAddr = "";
                else {
                    p += strlen(MACADDR_TAG);
                    if (!(p2 = strchr(p, '"'))) {
                        CloseSocket(sock);
                        return -1;
                    }
                    else if (p2 - p >= (int)sizeof(macAddrBuffer)) {
                        CloseSocket(sock);
                        return -1;
                    }
                    strncpy(macAddrBuffer, p, p2 - p);
                    macAddrBuffer[p2 - p] = '\0';
                    macAddr = macAddrBuffer;
                }
            
                if (show) {
                    if (name[0])
                        printf("Name: '%s', ", name);
                    printf("IP: %s", addressStr.c_str());
                    if (macAddr[0])
                        printf(", MAC: %s", macAddr);
                    printf("\n");
                }
                
                WiFiInfo info(name, addressStr);
                list.push_back(info);
            
                if (count > 0 && --count == 0) {
                    CloseSocket(sock);
                    return 0;
                }
            }
        }
        
        /* if we found at least one module, reset the retry counter */
        if (numberFound > 0)
            tries = DISCOVER_ATTEMPTS;
            
        /* otherwise, decrement the number of attempts remaining */
        else
            --tries;
    }
    
    /* close the socket */
    CloseSocket(sock);
    
    /* return successfully */
    return 0;
}

bool WiFiPropConnection::isOpen()
{
    return m_telnetSocket != INVALID_SOCKET;
}

int WiFiPropConnection::getVersion()
{
    uint8_t buffer[1024];
    int hdrCnt, result, srcLen;
    char *src, *dst;
    
    hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
GET /wx/setting?name=version HTTP/1.1\r\n\
\r\n");

    if (sendRequest(buffer, hdrCnt, buffer, sizeof(buffer), &result) == -1) {
        printf("error: get version failed\n");
        return -1;
    }
    else if (result != 200) {
        printf("error: get version returned %d\n", result);
        return -1;
    }
    
    src = (char *)buffer; 
    srcLen = strlen(src);
    
    while (srcLen >= 4) {
        if (src[0] == '\r' && src[1] == '\n' && src[2] == '\r' && src[3] == '\n')
            break;
        --srcLen;
        ++src;
    }
    if (srcLen <= 4) {
        printf("error: no version string\n");
        return -1;
    }
    
    if (!(dst = (char *)malloc(srcLen - 4 + 1))) {
        printf("error: insufficient memory for version string\n");
        return -1;
    }
    
    if (m_version)
        free(m_version);
    strcpy(dst, src + 4);
    m_version = dst;

    return 0;
}

int WiFiPropConnection::setName(const char *name)
{
    uint8_t buffer[1024];
    int hdrCnt, result;
    
    hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
POST /wx/setting?name=module-name&value=%s HTTP/1.1\r\n\
\r\n", name);

    if (sendRequest(buffer, hdrCnt, buffer, sizeof(buffer), &result) == -1) {
        printf("error: module-name update request failed\n");
        return -1;
    }
    else if (result != 200) {
        printf("error: module-name update returned %d\n", result);
        return -1;
    }

    hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
POST /wx/save-settings HTTP/1.1\r\n\
\r\n");

    if (sendRequest(buffer, hdrCnt, buffer, sizeof(buffer), &result) == -1) {
        printf("error: save settings request failed\n");
        return -1;
    }
    else if (result != 200) {
        printf("error: save settings returned %d\n", result);
        return -1;
    }

    return 0;
}

int WiFiPropConnection::generateResetSignal()
{
    uint8_t buffer[1024];
    int hdrCnt, result;
    
    hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
POST /wx/propeller/reset?reset-pin=%d HTTP/1.1\r\n\
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
    return SendSocketData(m_telnetSocket, buf, len);
}

int WiFiPropConnection::receiveDataTimeout(uint8_t *buf, int len, int timeout)
{
    if (!isOpen())
        return -1;
    return ReceiveSocketDataTimeout(m_telnetSocket, buf, len, timeout);
}

int WiFiPropConnection::receiveDataExactTimeout(uint8_t *buf, int len, int timeout)
{
    if (!isOpen())
        return -1;
    return ReceiveSocketDataExactTimeout(m_telnetSocket, buf, len, timeout);
}

int WiFiPropConnection::setBaudRate(int baudRate)
{
    uint8_t buffer[1024];
    int hdrCnt, result;
    
    if (baudRate != m_baudRate) {

        hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
POST /wx/setting?name=baud-rate&value=%d HTTP/1.1\r\n\
\r\n", baudRate);

        if (sendRequest(buffer, hdrCnt, buffer, sizeof(buffer), &result) == -1) {
            printf("error: set baud-rate request failed\n");
            return -1;
        }
        else if (result != 200) {
            printf("error: set baud-rate returned %d\n", result);
            return -1;
        }
    
        m_baudRate = baudRate;
    }
    
    return 0;
}

int WiFiPropConnection::terminal(bool checkForExit, bool pstMode)
{
    if (!isOpen())
        return -1;
    SocketTerminal(m_telnetSocket, checkForExit, pstMode);
    return 0;
}

int WiFiPropConnection::sendRequest(uint8_t *req, int reqSize, uint8_t *res, int resMax, int *pResult)
{
    SOCKET sock;
    char buf[80];
    int cnt;
    
    if (ConnectSocketTimeout(&m_httpAddr, CONNECT_TIMEOUT, &sock) != 0) {
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

