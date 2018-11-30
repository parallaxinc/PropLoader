#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wifipropconnection.h"
#include "loader.h"
#include "proploader.h"

#define CALIBRATE_DELAY 10

#define HTTP_PORT       80
#define TELNET_PORT     23
#define DISCOVER_PORT   32420

WiFiPropConnection::WiFiPropConnection()
    : m_ipaddr(NULL),
      m_version(NULL),
      m_telnetSocket(INVALID_SOCKET),
      m_resetPin(12)
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

    if (GetInternetAddress(m_ipaddr, HTTP_PORT, &m_httpAddr) != 0)
        return -1;

    if (GetInternetAddress(m_ipaddr, TELNET_PORT, &m_telnetAddr) != 0)
        return -1;

    setPortName(ipaddr);

    return 0;
}

int WiFiPropConnection::checkVersion()
{
    int versionOkay;
    versionOkay = strncmp(m_version, WIFI_REQUIRED_MAJOR_VERSION, strlen(WIFI_REQUIRED_MAJOR_VERSION)) == 0
               || strncmp(m_version, WIFI_REQUIRED_MAJOR_VERSION_LEGACY, strlen(WIFI_REQUIRED_MAJOR_VERSION_LEGACY)) == 0;
    return versionOkay ? 0 : -1;
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

static int beginsWith(const char *body, const char *str)
{
    int length = strlen(str);
    return strncasecmp(body, str, length) == 0;
}
    
int WiFiPropConnection::loadImage(const uint8_t *image, int imageSize, uint8_t *response, int responseSize)
{
    uint8_t buffer[1024], *packet, *body;
    int hdrCnt, result, cnt;
    int loaderBaudRate;
    
    if (!GetNumericConfigField(config(), "loader-baud-rate", &loaderBaudRate))
        loaderBaudRate = DEF_LOADER_BAUDRATE;
        
    /* use the initial loader baud rate */
    if (setBaudRate(loaderBaudRate) != 0) 
        return -1;
        
    hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
POST /propeller/load?baud-rate=%d&reset-pin=%d&response-size=%d&response-timeout=1000 HTTP/1.1\r\n\
Content-Length: %d\r\n\
\r\n", loaderBaudRate, m_resetPin, responseSize, imageSize);

    if (!(packet = (uint8_t *)malloc(hdrCnt + imageSize)))
        return -1;

    memcpy(packet,  buffer, hdrCnt);
    memcpy(&packet[hdrCnt], image, imageSize);
    
    if ((cnt = sendRequest(packet, hdrCnt + imageSize, buffer, sizeof(buffer) - 1, &result)) == -1) {
        message("Load request failed");
        return -1;
    }
    else if (result != 200) {
        char *body = (char *)getBody(buffer, cnt, &cnt);
        int sts = -1;
        if (body) {
            body[cnt] = '\0';
            if (beginsWith(body, "RX handshake timeout")) {
                nerror(ERROR_COMMUNICATION_LOST);
            }
            else if (beginsWith(body, "RX handshake failed")) {
                nerror(ERROR_PROPELLER_NOT_FOUND, portName());
            }
            else if (beginsWith(body, "Wrong Propeller version: got ")) {
                int version = atoi(&body[strlen("Wrong Propeller version: got ")]);
                nerror(ERROR_WRONG_PROPELLER_VERSION, version);
            }
            else if (beginsWith(body, "Checksum timeout")) {
                nerror(ERROR_COMMUNICATION_LOST);
            }
            else if (beginsWith(body, "Checksum error")) {
                nerror(ERROR_RAM_CHECKSUM_FAILED);
            }
            else if (beginsWith(body, "Load image failed")) {
                nerror(ERROR_LOAD_IMAGE_FAILED);
            }
            else if (beginsWith(body, "StartAck timeout")) {
                nerror(ERROR_COMMUNICATION_LOST);
                sts = -2;
            }
            else {
                nerror(ERROR_INTERNAL_CODE_ERROR);
            }
        }
        message("Load returned %d", result);
        return sts;
    }
    
    /* find the response body */
    if (!(body = getBody(buffer, cnt, &cnt))) {
        nerror(ERROR_COMMUNICATION_LOST);
        return -2;
    }

    /* copy the body to the response if it fits */
    if (cnt != responseSize) {
        nerror(ERROR_COMMUNICATION_LOST);
        return -2;
    }
    memcpy(response, body, cnt);
        
    return 0;
}

int WiFiPropConnection::loadImage(const uint8_t *image, int imageSize, LoadType loadType, int info)
{
    uint8_t buffer[1024], *packet;
    int hdrCnt, result, cnt;
    int loaderBaudRate;
    
    if (!GetNumericConfigField(config(), "loader-baud-rate", &loaderBaudRate))
        loaderBaudRate = DEF_LOADER_BAUDRATE;
    
    /* WX image buffer is limited to 2K */
    if (imageSize > 2048)
        return -1;
    
    /* use the initial loader baud rate */
    if (setBaudRate(loaderBaudRate) != 0) 
        return -1;
        
    hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
POST /propeller/load?baud-rate=%d HTTP/1.1\r\n\
Content-Length: %d\r\n\
\r\n", loaderBaudRate, imageSize);

    if (!(packet = (uint8_t *)malloc(hdrCnt + imageSize)))
        return -1;

    memcpy(packet,  buffer, hdrCnt);
    memcpy(&packet[hdrCnt], image, imageSize);
    
    if ((cnt = sendRequest(packet, hdrCnt + imageSize, buffer, sizeof(buffer), &result)) == -1) {
        message("Load request failed");
        return -1;
    }
    else if (result != 200) {
        char *body = (char *)getBody(buffer, cnt, &cnt);
        int sts = -1;
        if (body) {
            body[cnt] = '\0';
            if (beginsWith(body, "RX handshake timeout")) {
                nerror(ERROR_COMMUNICATION_LOST);
            }
            else if (beginsWith(body, "RX handshake failed")) {
                nerror(ERROR_PROPELLER_NOT_FOUND, portName());
            }
            else if (beginsWith(body, "Wrong Propeller version: got ")) {
                int version = atoi(&body[strlen("Wrong Propeller version: got ")]);
                nerror(ERROR_WRONG_PROPELLER_VERSION, version);
            }
            else if (beginsWith(body, "Checksum timeout")) {
                nerror(ERROR_COMMUNICATION_LOST);
            }
            else if (beginsWith(body, "Checksum error")) {
                nerror(ERROR_RAM_CHECKSUM_FAILED);
            }
            else if (beginsWith(body, "Load image failed")) {
                nerror(ERROR_LOAD_IMAGE_FAILED);
            }
            else {
                nerror(ERROR_INTERNAL_CODE_ERROR);
            }
        }
        message("Load returned %d", result);
        return sts;
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
        message("GetInterfaceAddresses failed");
        return -1;
    }
    
    /* create a broadcast socket */
    if (OpenBroadcastSocket(DISCOVER_PORT, &sock) != 0) {
        message("OpenBroadcastSocket failed");
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
                message("SendSocketDataTo failed");
                CloseSocket(sock);
                return -1;
            }
        }
    
        /* receive wifi module responses */
        while (SocketDataAvailableP(sock, DISCOVER_REPLY_TIMEOUT)) {

            /* get the next response */
            if ((cnt = ReceiveSocketDataAndAddress(sock, rxBuf, sizeof(rxBuf) - 1, &addr)) < 0) {
                message("ReceiveSocketData failed");
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
                        message("Skipping duplicate: %s", AddressToString(&addr));
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
            
                message("From %s got: %s", AddressToString(&addr), rxBuf);
                
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
    uint8_t buffer[1024], *body;
    int hdrCnt, result, cnt;
    char *dst;
    
    hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
GET /wx/setting?name=version HTTP/1.1\r\n\
\r\n");

    if ((cnt = sendRequest(buffer, hdrCnt, buffer, sizeof(buffer), &result)) == -1) {
        message("Get version failed");
        return -1;
    }
    else if (result != 200) {
        message("Get version returned %d", result);
        return -1;
    }
    
    if (!(body = getBody(buffer, cnt, &cnt)))
        return -1;

    if (cnt <= 0) {
        message("No version string");
        return -1;
    }
    
    if (!(dst = (char *)malloc(cnt + 1))) {
        nmessage(ERROR_INSUFFICIENT_MEMORY);
        return -1;
    }
    
    if (m_version)
        free(m_version);
    strncpy(dst, (char *)body, cnt);
    body[cnt] = '\0';
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
        message("module-name update request failed");
        return -1;
    }
    else if (result != 200) {
        message("module-name update returned %d", result);
        return -1;
    }

    hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
POST /wx/save-settings HTTP/1.1\r\n\
\r\n");

    if (sendRequest(buffer, hdrCnt, buffer, sizeof(buffer), &result) == -1) {
        message("save-settings request failed");
        return -1;
    }
    else if (result != 200) {
        message("save-settings returned %d", result);
        return -1;
    }

    return 0;
}

int WiFiPropConnection::setResetMethod(const char *method)
{
    if (strcmp(method, "dtr") == 0)
        m_resetPin = 12;
    else if (strcmp(method, "rts") == 0)
        m_resetPin = 15;
    else if (isdigit(*method))
        m_resetPin = atoi(method);
    else
        return -1;
    return 0;
}

int WiFiPropConnection::generateResetSignal()
{
    uint8_t buffer[1024];
    int hdrCnt, result;
    
    hdrCnt = snprintf((char *)buffer, sizeof(buffer), "\
POST /propeller/reset?reset-pin=%d HTTP/1.1\r\n\
\r\n", m_resetPin);

    if (sendRequest(buffer, hdrCnt, buffer, sizeof(buffer), &result) == -1) {
        message("reset request failed");
        return -1;
    }
    else if (result != 200) {
        message("reset returned %d", result);
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
            message("Set baud-rate request failed");
            return -1;
        }
        else if (result != 200) {
            message("Set baud-rate returned %d", result);
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
        message("Connect failed");
        return -1;
    }
    
    if (verbose > 1) {
        printf("REQ: %d\n", reqSize);
        dumpHdr(req, reqSize);
    }
    
    if (SendSocketData(sock, req, reqSize) != reqSize) {
        message("Send request failed");
        CloseSocket(sock);
        return -1;
    }
    
    cnt = ReceiveSocketDataTimeout(sock, res, resMax, RESPONSE_TIMEOUT);
    CloseSocket(sock);

    if (cnt == -1) {
        message("Receive response failed");
        return -1;
    }
    
    if (verbose > 1) {
        printf("RES: %d\n", cnt);
        dumpResponse(res, cnt);
    }
    
    if (sscanf((char *)res, "%s %d", buf, pResult) != 2)
        return -1;
        
    return cnt;
}
    
uint8_t *WiFiPropConnection::getBody(uint8_t *msg, int msgSize, int *pBodySize)
{
    uint8_t *p = msg;
    int cnt = msgSize;

    /* find the message body */
    while (cnt >= 4 && (p[0] != '\r' || p[1] != '\n' || p[2] != '\r' || p[3] != '\n')) {
        --cnt;
        ++p;
    }
    
    /* make sure we found the \r\n\r\n that terminates the header */
    if (cnt < 4)
        return NULL;

    /* return the body */
    *pBodySize = cnt - 4;
    return p + 4;
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

