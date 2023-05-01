
#define MAX_FILESIZE 2000000

/*
 *  returns 0 on success
 */
int speakText(char* text, char* voice, char* subscriptionKey, char* region, int volume, int soundDeviceId, int maxDurationMs) {
    if (volume == 0)
        return 0;

    static sslsocket* pSpeechSocket = 0;
    static ULONGLONG accessTokenTimestamp = 0;
    static char accessToken[1024];
    static char* buf = 0;

    if (buf == 0)
        buf = malloc(MAX_FILESIZE);

    //get a new access token if the current one is older than 10 minutes
    if (GetTickCount64() - accessTokenTimestamp > 540000) {
        sslsocket tokenSocket;
        buf[0] = 0;
        str_vacat(buf, 2, region, ".api.cognitive.microsoft.com");
        if (sslsock_connect(&tokenSocket, buf, "443") != 0)
            return -1;
        buf[0] = 0;
        unsigned int len = str_vacat(buf,5, "POST /sts/v1.0/issueToken HTTP/1.1\r\nHost: ", region,
        ".api.cognitive.microsoft.com\r\nUser-Agent: caTTS\r\nConnection: close\r\nOcp-Apim-Subscription-Key: ", subscriptionKey,
        "\r\nContent-type: application/x-www-form-urlencoded\r\nContent-Length: 0\r\n\r\n");
        if (sslsock_send(&tokenSocket, buf, len) != 0)
            return -2;
        int iRes = sslsock_recv_http(&tokenSocket, buf, MAX_FILESIZE);
        if (memcmp(buf, "HTTP/1.1 200 OK", 15) != 0) {
            char* msgStart = strstr(buf, "message\":");
            if (msgStart) {
                msgStart += 11;
                *strstr(msgStart, "\"") = 0;
                MessageBoxA(NULL, msgStart, "caTTS Azure error", MB_SETFOREGROUND);
            }
            return -3;
        }
        int headerLen = (int)(strstr(buf, "\r\n\r\n") - buf) + 4;
        accessToken[0] = 0;
        str_cat(accessToken, buf + headerLen);
        sslsock_disconnect(&tokenSocket);
        accessTokenTimestamp = GetTickCount64();
    }


    for (int i = 0; i < 3; i++) {
        if (pSpeechSocket == NULL) {
            pSpeechSocket = malloc(sizeof(sslsocket));
            buf[0] = 0;
            str_vacat(buf, 2, region, ".tts.speech.microsoft.com");
            if (sslsock_connect(pSpeechSocket, buf, "443") != 0)
                continue;
        }
        char* ssmlOffset = buf + 2048;
        ssmlOffset[0] = 0;
        int ssmlLen = str_vacat(ssmlOffset, 5, "<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xmlns:mstts='https://www.w3.org/2001/mstts' xml:lang='en-US'><voice name='",voice,
        "'>",text,"</voice></speak>");
        buf[0] = 0;
        int len = str_vacat(buf, 5, "POST /cognitiveservices/v1 HTTP/1.1\r\nHost: ",region,
        ".tts.speech.microsoft.com\r\nUser-Agent: caTTS\r\nConnection: keep-alive\r\nX-Microsoft-OutputFormat: riff-24khz-16bit-mono-pcm\r\nAuthorization: Bearer ",
        accessToken,"\r\nContent-Type: application/ssml+xml\r\nContent-Length: ");
        len += uint_to_str(ssmlLen, buf + len);
        len += str_cat(buf + len, "\r\n\r\n");
        if ((sslsock_send(pSpeechSocket, buf, len) == 0) && (sslsock_send(pSpeechSocket, ssmlOffset, ssmlLen) == 0)) {
            int iRes = sslsock_recv_http(pSpeechSocket, buf, MAX_FILESIZE);
            if (iRes > 0) {
                if (memcmp(buf, "HTTP/1.1 200 OK", 15) != 0)
                    return -4;
                int headerLen = (int)(strstr(buf, "\r\n\r\n") - buf) + 4;
                ds_playsound(soundDeviceId, NULL, buf + headerLen, iRes - headerLen, volume, maxDurationMs);
                return 0;
            }
        }
        sslsock_disconnect(pSpeechSocket);
        free(pSpeechSocket);
        pSpeechSocket = 0;
    }

    return -5;

}