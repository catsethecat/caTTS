// sslsocket.c v2

#include <WinSock2.h>
#include <WS2tcpip.h>
#define SECURITY_WIN32
#include <security.h>
#include <schnlsp.h>

#pragma comment (lib,"Ws2_32.lib")
#pragma comment (lib,"Secur32.lib")

typedef struct {
    //char* address;
    SOCKET socket;
    CtxtHandle hCtxt;
    CredHandle hCred;
}sslsocket;


/*
 * receives complete TLS records (one or more)
 * returns total number of bytes received
 */
int sslsock_recv_tls_blocks(sslsocket* psslsock, unsigned char* buf, int buflen) {
    int totalReceived = 0, target = 0;
    do {
        int recvCount = recv(psslsock->socket, buf + totalReceived, buflen - totalReceived, 0);
        if (recvCount <= 0)
            return (recvCount == 0) ? 0 : -1;
        totalReceived += recvCount;
        for (; totalReceived >= target + 5; target += (buf[target + 3] * 256 + buf[target + 4]) + 5);
    } while (totalReceived != target);
    return totalReceived;
}

/*
 * initializes and connects a sslsocket.
 * returns 0 on success
 */
int sslsock_connect( sslsocket* psslsock, char* address, char* port) {
    static int wsockInit = 0;
    if (!wsockInit) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            return -1;
        wsockInit = 1;
    }
    //psslsock->address = address;
    struct addrinfo* serverAddr = NULL;
    struct addrinfo hints = { 0 };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (getaddrinfo(address, port, &hints, &serverAddr) != 0)
        return -2;
    psslsock->socket = socket(serverAddr->ai_family, serverAddr->ai_socktype, serverAddr->ai_protocol);
    if (psslsock->socket == INVALID_SOCKET)
        return -3;
    if (connect(psslsock->socket, serverAddr->ai_addr, (int)serverAddr->ai_addrlen) != 0)
        return -4;
    freeaddrinfo(serverAddr);
    SECURITY_STATUS secStatus;
    SCHANNEL_CRED chCred = { 0 };
    chCred.dwVersion = SCHANNEL_CRED_VERSION;
    secStatus = AcquireCredentialsHandleA(NULL, UNISP_NAME_A, SECPKG_CRED_OUTBOUND, NULL, &chCred, NULL, NULL, &psslsock->hCred, NULL);
    if (secStatus != SEC_E_OK)
        return -5;
    SecBuffer outSecBuffers[2] = { 0 }, inSecBuffers[2] = { 0 };
    SecBufferDesc outSecBufferDesc = { .ulVersion = SECBUFFER_VERSION, .cBuffers = 2, .pBuffers = outSecBuffers };
    SecBufferDesc inSecBufferDesc = { .ulVersion = SECBUFFER_VERSION, .cBuffers = 2, .pBuffers = inSecBuffers };
    outSecBuffers[0].BufferType = SECBUFFER_TOKEN;
    outSecBuffers[1].BufferType = SECBUFFER_ALERT;
    inSecBuffers[0].BufferType = SECBUFFER_TOKEN;
    inSecBuffers[1].BufferType = SECBUFFER_EMPTY;
    unsigned char recvBuf[65536];
    inSecBuffers[0].pvBuffer = recvBuf;
    DWORD contextAttribs;
    secStatus = InitializeSecurityContextA(&psslsock->hCred, NULL, address, ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY, 0, 0, NULL, 0, &psslsock->hCtxt, &outSecBufferDesc, &contextAttribs, NULL);
    while (secStatus == SEC_I_CONTINUE_NEEDED || secStatus == SEC_E_INCOMPLETE_MESSAGE) {
        if (outSecBuffers[0].cbBuffer > 0) {
            if (send(psslsock->socket, outSecBuffers[0].pvBuffer, outSecBuffers[0].cbBuffer, 0) == SOCKET_ERROR)
                return -6;
            FreeContextBuffer(outSecBuffers[0].pvBuffer);
            outSecBuffers[0].cbBuffer = 0;
        }
        inSecBuffers[0].cbBuffer = sslsock_recv_tls_blocks(psslsock, recvBuf, 65536);
        secStatus = InitializeSecurityContextA(&psslsock->hCred, &psslsock->hCtxt, address, ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY, 0, 0, &inSecBufferDesc, 0, NULL, &outSecBufferDesc, &contextAttribs, NULL);
    }
    if (secStatus != SEC_E_OK)
        return -7;
    return 0;
}

/*
 * disconnects and cleans up a sslsocket.
 * returns 0 on success
 */
int sslsock_disconnect( sslsocket* psslsock) {
    /*DWORD token = SCHANNEL_SHUTDOWN;
    SecBuffer secBuffer = { .BufferType = SECBUFFER_TOKEN, .cbBuffer = sizeof(token), .pvBuffer = &token };
    SecBufferDesc secBufferDesc = { .ulVersion = SECBUFFER_VERSION, .cBuffers = 1, .pBuffers = &secBuffer };
    SECURITY_STATUS secStatus = ApplyControlToken(&psslsock->hCtxt, &secBufferDesc);
    if (secStatus != SEC_E_OK)
        return -1;
    SecBuffer outSecBuffers[2] = { 0 }, inSecBuffers[2] = { 0 };
    SecBufferDesc outSecBufferDesc = { .ulVersion = SECBUFFER_VERSION, .cBuffers = 2, .pBuffers = outSecBuffers };
    SecBufferDesc inSecBufferDesc = { .ulVersion = SECBUFFER_VERSION, .cBuffers = 2, .pBuffers = inSecBuffers };
    outSecBuffers[0].BufferType = SECBUFFER_TOKEN;
    outSecBuffers[1].BufferType = SECBUFFER_ALERT;
    inSecBuffers[0].BufferType = SECBUFFER_TOKEN;
    inSecBuffers[1].BufferType = SECBUFFER_EMPTY;
    char recvBuf[65536];
    inSecBuffers[0].pvBuffer = recvBuf;
    DWORD contextAttribs;
    while ((secStatus = InitializeSecurityContextA(&psslsock->hCred, &psslsock->hCtxt, psslsock->address, ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY, 0, 0, &inSecBufferDesc, 0, NULL, &outSecBufferDesc, &contextAttribs, NULL)) == SEC_I_CONTINUE_NEEDED || secStatus == SEC_E_INCOMPLETE_MESSAGE) {
        if (outSecBuffers[0].cbBuffer > 0) {
            if (send(psslsock->socket, outSecBuffers[0].pvBuffer, outSecBuffers[0].cbBuffer, 0) == SOCKET_ERROR)
                return -2;
            FreeContextBuffer(outSecBuffers[0].pvBuffer);
            outSecBuffers[0].cbBuffer = 0;
        }
        inSecBuffers[0].cbBuffer = sslsock_recv_tls_blocks(psslsock, recvBuf, 65536);
    }
    if (!(secStatus == SEC_I_CONTEXT_EXPIRED || secStatus == SEC_E_OK))
        return -3;*/
    if (psslsock->socket != 0) {
        DeleteSecurityContext(&psslsock->hCtxt);
        FreeCredentialsHandle(&psslsock->hCred);
        shutdown(psslsock->socket, SD_SEND);
        closesocket(psslsock->socket);
        memset(psslsock, 0, sizeof( sslsocket));
    }
    return 0;
}

/*
 * returns 0 on success
 */
int sslsock_send( sslsocket* psslsock, char* buf, unsigned int buflen) {
    SecPkgContext_StreamSizes sizes;
    SECURITY_STATUS secStatus = QueryContextAttributes(&psslsock->hCtxt, SECPKG_ATTR_STREAM_SIZES, &sizes);
    if (secStatus != SEC_E_OK)
        return -1;
    char data[65536];
    SecBuffer secBuffers[4] = {
        {.BufferType = SECBUFFER_STREAM_HEADER, .cbBuffer = sizes.cbHeader, .pvBuffer = data},
        {.BufferType = SECBUFFER_DATA, .cbBuffer = 0, .pvBuffer = data + sizes.cbHeader},
        {.BufferType = SECBUFFER_STREAM_TRAILER, .cbBuffer = sizes.cbTrailer, .pvBuffer = 0},
        {.BufferType = SECBUFFER_EMPTY, .cbBuffer = 0, .pvBuffer = 0}
    };
    SecBufferDesc secBufferDesc = { .ulVersion = SECBUFFER_VERSION, .cBuffers = 4, .pBuffers = secBuffers };
    unsigned int totalSent = 0;
    while (totalSent < buflen) {
        int bytesToSend = min(buflen - totalSent, sizes.cbMaximumMessage);
        secBuffers[1].cbBuffer = bytesToSend;
        memcpy(data + sizes.cbHeader, buf + totalSent, bytesToSend);
        secBuffers[2].pvBuffer = data + sizes.cbHeader + bytesToSend;
        if (EncryptMessage(&psslsock->hCtxt, 0, &secBufferDesc, 0) != SEC_E_OK)
            return -2;
        if (send(psslsock->socket, data, secBuffers[0].cbBuffer + secBuffers[1].cbBuffer + secBuffers[2].cbBuffer, 0) == SOCKET_ERROR)
            return -3;
        totalSent += bytesToSend;
    }
    return 0;
}

/*
 * returns: number of bytes received or 0 if connection was closed by server
 */
int sslsock_recv( sslsocket* psslsock, unsigned char* buf, int buflen) {
    int totalReceived = sslsock_recv_tls_blocks(psslsock, buf, buflen);
    SecBuffer secBuffers[4];
    SecBufferDesc secBufferDesc = { .ulVersion = SECBUFFER_VERSION, .cBuffers = 4, .pBuffers = secBuffers };
    int totalDecrypted = 0;
    for (int pos = 0; pos < totalReceived;) {
        unsigned short len = (buf[pos + 3] * 256 + buf[pos + 4]) + 5;
        if (buf[pos] == 0x17) {
            secBuffers[0].BufferType = SECBUFFER_DATA;
            secBuffers[0].cbBuffer = len;
            secBuffers[0].pvBuffer = buf + pos;
            secBuffers[1].BufferType = secBuffers[2].BufferType = secBuffers[3].BufferType = SECBUFFER_EMPTY;
            if (DecryptMessage(&psslsock->hCtxt, &secBufferDesc, 0, NULL) != SEC_E_OK)
                return -1;
            memcpy(buf + totalDecrypted, secBuffers[1].pvBuffer, secBuffers[1].cbBuffer);
            totalDecrypted += secBuffers[1].cbBuffer;
        }
        pos += len;
    }
    buf[totalDecrypted] = 0;
    return totalDecrypted;
}

/*
 * returns: number of bytes received or 0 if connection was closed by server
 */
unsigned int sslsock_recv_http( sslsocket* psslsock, unsigned char* buf, unsigned int buflen) {
    buflen--;
    int res = 0;
    unsigned int totalReceived = 0, chunked = 0, target = 0, offset = 0;
    char* content = 0, * pCharTemp;
    while ((res = sslsock_recv(psslsock, buf + totalReceived, buflen - totalReceived)) > 0) {
        totalReceived += res;
        buf[totalReceived] = 0;
        if (content == 0 && (content = strstr(buf, "\r\n\r\n")) != 0) {
            chunked = (strstr(buf, "Transfer-Encoding: chunked") != 0);
            if (pCharTemp = strstr(buf, "Content-Length:"))
                target = str_getint(pCharTemp + 16);//sscanf_s(pCharTemp + 16, "%i", &target);
            target += (int)(content - buf + 4);
        }
        while (chunked && totalReceived > target) {
            int value = 0, hexlen = 0;
            value = str_gethex(buf + target, &hexlen);//sscanf_s(buf + target, "%x%n", &value, &hexlen);
            if (totalReceived < target + hexlen + 2)
                break;
            if (value == 0)
                return target - 2;
            memcpy(buf + target - offset, buf + target + hexlen + 2, totalReceived - (target + hexlen + 2));
            totalReceived -= (hexlen + 2 + offset);
            target += value + 2 - offset;
            offset = 2;
        }
        if (!chunked && totalReceived == target)
            return totalReceived;
    }
    return totalReceived;
}