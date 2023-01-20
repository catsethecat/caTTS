#define WIN32_LEAN_AND_MEAN
#define TTS_QUEUE_COUNT 10
#define TTS_MAX_MESSAGE_LEN 1024

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <ShlObj.h>

#include "stringstuff.c"
#include "sslsocket.c"
#include "iniparser.c"
#include "directsound.c"
#include "azurespeech.c"

#pragma comment (lib, "kernel32.lib")
#pragma comment (lib, "user32")
#pragma comment (lib, "shell32")
#pragma comment (lib, "gdi32")

typedef struct {
	char message[TTS_MAX_MESSAGE_LEN];
	char* voice;
	char* pitch;
}ttsMessage;

ttsMessage ttsQueue[TTS_QUEUE_COUNT];
int tts_playback_pos = 0;
int tts_last_message_pos = 0;
int ttsBusy = 0;
char configFolder[MAX_PATH];
HMENU hMenu;
char* statusLines[2] = {"line0", "line1"};
HANDLE hThread0;
HANDLE hThread1;
HANDLE hThread2;
sslsocket ircsock;
sslsocket pubsock;
inifile config;
int voiceDeviceId;
int effectDeviceId;

/*
	-add option to limit tts message length
	-add chat commands to allow moderators to control tts
	-fix jank and better error checking
	-other stuff i forgor
*/

void fatalError(char* msg) {
	MessageBoxA(NULL, msg, "fatal error", 0);
	ExitProcess(0);
}

void ttsThread() {
	if (ttsBusy)
		return;
	ttsBusy = 1;
	while (ttsQueue[tts_playback_pos].message[0] != 0) {
		int res = speakText(ttsQueue[tts_playback_pos].message,
			ttsQueue[tts_playback_pos].voice,
			ttsQueue[tts_playback_pos].pitch,
			iniGetValue(&config, "Azure", "SubscriptionKey"),
			iniGetValue(&config, "Azure", "Region"),
			str_getint(iniGetValue(&config, "Misc", "VoiceVolume")),
			voiceDeviceId);

		ttsQueue[tts_playback_pos].message[0] = 0;
		tts_playback_pos = (tts_playback_pos + 1) % TTS_QUEUE_COUNT;
	}
	tts_playback_pos = tts_last_message_pos;
	ttsBusy = 0;
}

void ttsAddMessage(char* voice, char* pitch, int argc, ...) {
	va_list ap;
	va_start(ap, argc);
	char* c = ttsQueue[tts_last_message_pos].message;
	for(int i = 0; i < argc; i++)
		c += str_cat(c, va_arg(ap,char*));
	va_end(ap);
	ttsQueue[tts_last_message_pos].voice = voice;
	ttsQueue[tts_last_message_pos].pitch = pitch;
	tts_last_message_pos = (tts_last_message_pos + 1) % TTS_QUEUE_COUNT;
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ttsThread, NULL, 0, NULL);
}

// string must be static its never copied atm
void updateContextMenuLine(int line, char* string) {
	statusLines[line] = string;
	ModifyMenuA(hMenu, line, MF_STRING | MF_DISABLED, 0, statusLines[line]);
}

DWORD WINAPI Thread0(LPVOID lpParam) {
	while (1) {
		Sleep(60000);
		if (sslsock_send(&pubsock, "\x81\xfe\x0\x10\x0\x0\x0\x0{\"type\": \"PING\"}", 24) != 0)
			return 0;
	}
	return 0;
}

DWORD WINAPI Thread1(LPVOID lpParam) {
	updateContextMenuLine(1, "redeems: connecting...");
	sslsock_disconnect(&pubsock);
	if (sslsock_connect(&pubsock, "pubsub-edge.twitch.tv", "443") != 0)
		goto Thread1End;
	unsigned char buf[65536] = { 0 };
	if (sslsock_send(&pubsock,
		"GET / HTTP/1.1\r\n"
		"Host: pubsub-edge.twitch.tv\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
		"Sec-WebSocket-Version: 13\r\n\r\n", 160) != 0)
		goto Thread1End;
	int iRes = sslsock_recv(&pubsock, buf, 65536);
	if (memcmp(buf, "HTTP/1.1 101 Switching Protocols", 32) != 0)
		goto Thread1End;
	char* pass = iniGetValue(&config, "Twitch", "OAuthToken");
	*(buf + 8) = 0;
	unsigned short dataLen = (unsigned short)str_vacat(buf + 8, 5, "{\"type\": \"LISTEN\",\"nonce\": \"44h1k13746815ab1r2\",\"data\": {\"topics\": [\"channel-points-channel-v1.",
	iniGetValue(&config, "Twitch", "ChannelID"),"\"],\"auth_token\": \"",pass,"\"}}");
	memset(buf, 0, 8);
	*(unsigned short*)buf = 0xFE81;
	buf[2] = ((unsigned char*)&dataLen)[1];
	buf[3] = ((unsigned char*)&dataLen)[0];
	if (sslsock_send(&pubsock, buf, 8 + dataLen) != 0)
		goto Thread1End;
	hThread0 = CreateThread(0, 0, Thread0, NULL, 0, NULL);
	int res = 0;
	while ((res = sslsock_recv(&pubsock, buf, 65536)) > 0) {
		buf[res] = 0;
		char* payload = buf + (buf[1] == 126 ? 4 : (buf[1] == 127 ? 10 : 2));
		if (strstr(payload, "\"RESPONSE\"")) {
			char* err = strstr(payload, "\"error\":\"") + 9;
			*strchr(err, '\"') = 0;
			if (*err) {
				//buf[0] = 0;
				//str_vacat(buf, 2, "redeems: ", err);
				updateContextMenuLine(1, "redeems: error");
				return 0;
			}
			updateContextMenuLine(1, "redeems: connected");
		}
		if (strstr(payload, "reward-redeemed")) {
			char* username = strstr(payload, "\\\"login\\\":\\\"") + 12;
			char* title = strstr(payload, "\\\"title\\\":\\\"") + 12;
			char* cost = strstr(payload, "\\\"cost\\\":") + 9;
			*strchr(username, '\\') = 0;
			*strchr(title, '\\') = 0;
			*strchr(cost, ',') = 0;
			char* nickname = iniGetValue(&config, "Nicknames", username);
			if (nickname)
				username = nickname;
			char* soundPath = iniGetValue(&config, "SoundEffects", title);
			if (soundPath) {
				ds_playsound(effectDeviceId, soundPath, NULL, str_getint(iniGetValue(&config, "Misc", "SoundEffectVolume")));
				continue;
			}
			char* format = iniGetValue(&config, "RedeemMessages", title);
			if(!format)
				format = iniGetValue(&config, "RedeemMessages", "Default");
			if (!format || !*format)
				continue;
			char* tmp = buf + 65000;
			*tmp = 0;
			str_cat(tmp, format);
			strrep(tmp, "%name", username);
			strrep(tmp, "%item", title);
			strrep(tmp, "%cost", cost);
			ttsAddMessage(iniGetValue(&config, "Misc", "DefaultVoice"), "+0Hz",1, tmp);
		}
	}
	Thread1End:
	updateContextMenuLine(1, "redeems: stopped");
	return 0;
}

void AddThirdPartyEmotes(sslsocket* psock, char* host, char* resource, char* prefix, char suffix, char* dstbuf) {
	unsigned char buf[65536];
	sslsock_disconnect(psock);
	if (sslsock_connect(psock, host, "443") == 0) {
		char req[256] = { 0 };
		int len = str_vacat(req, 5, "GET ",resource," HTTP/1.1\r\nHost: ",host,"\r\n\r\n");
		if (sslsock_send(psock, req, len) == 0) {
			int iRes = sslsock_recv_http(psock, buf, 65536);
			if (memcmp(buf, "HTTP/1.1 200 OK", 15) == 0) {
				for (char* start = buf, *end = buf; start = strstr(end + 1, prefix);) {
					start += strlen(prefix);
					end = strchr(start, suffix);
					*end = 0;
					str_vacat(dstbuf, 2, start, " ");
				}
			}
		}
	}
	sslsock_disconnect(&ircsock);
}

DWORD WINAPI Thread2(LPVOID lpParam) {
	updateContextMenuLine(0, "checking oauth token...");
	unsigned char buf[65536] = { 0 };

	for (int attempt = 0; ; attempt++) {
		sslsocket apisock;
		if (sslsock_connect(&apisock, "api.twitch.tv", "443") != 0)
			return -5;
		buf[0] = 0;
		str_vacat(buf, 3, "GET /helix/users HTTP/1.1\r\n"
			"Host: api.twitch.tv\r\n"
			"Client-Id: rc1t51ax9aj7r4m4nyjbnz2ri1e0a0\r\n"
			"Authorization: Bearer ", iniGetValue(&config, "Twitch", "OAuthToken"), "\r\n\r\n");

		if (sslsock_send(&apisock, buf, (unsigned int)strlen(buf)) != 0)
			return -6;
		int len = sslsock_recv(&apisock, buf, 65536);
		sslsock_disconnect(&apisock);
		if (memcmp(buf, "HTTP/1.1 200 OK", 10) != 0) {
			if (MessageBoxA(NULL, "Failed to retrieve channel information. This is most likely caused by an invalid or expired OAuth token. Press OK to attempt to get a new token.", "caTTS Error", MB_OKCANCEL) != IDOK)
				ExitProcess(0);
			ShellExecuteA(NULL, "open", "https://id.twitch.tv/oauth2/authorize?response_type=token&client_id=rc1t51ax9aj7r4m4nyjbnz2ri1e0a0&redirect_uri=http://localhost:3447&scope=chat%3Aread+chat%3Aedit+channel%3Aread%3Aredemptions", NULL, NULL, SW_SHOW);
			SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (serverSocket == -1)
				return -2;
			struct sockaddr_in fromAddr;
			fromAddr.sin_family = AF_INET;
			fromAddr.sin_port = htons(3447);
			fromAddr.sin_addr.s_addr = INADDR_ANY;
			int addrLen = sizeof(struct sockaddr_in);
			if (bind(serverSocket, (struct sockaddr*)&fromAddr, addrLen) != 0)
				return -3;
			if (listen(serverSocket, 10) != 0)
				return -4;
			SOCKET clientSocket = accept(serverSocket, NULL, NULL);
			for (int tmp, len = 0; (tmp = recv(clientSocket, buf + len, 65536, 0)) > 0; len += tmp) {
				buf[len + tmp] = 0;
				if (strstr(buf, "\r\n\r\n") > 0) {
					char* found = strstr(buf, "access_token");
					if (found) {
						send(clientSocket, "HTTP/1.1 200 OK\r\nContent-Length: 45\r\nContent-Type: text/html\r\n\r\n<html>oauth token updated successfully</html>", 109, 0);
						found[43] = 0;
						iniSetValue(&config, "Twitch", "OAuthToken", found + 13);
						break;
					}
					else {
						send(clientSocket, "HTTP/1.1 200 OK\r\nContent-Length: 144\r\nContent-Type: text/html\r\n\r\n<html><body>redirecting...</body><script>window.location.replace(\"http://localhost:3447/&\" + window.location.hash.substring(1));</script></html>", 209, 0);
						len = tmp = 0;
					}
				}
			}
			closesocket(clientSocket);
			closesocket(serverSocket);
			continue;
		}
		if (attempt > 0) {
			char* id = strstr(buf, "\"id\":\"") + 6;
			char* name = strstr(buf, "\"login\":\"") + 9;
			if (id == (char*)6 || name == (char*)9) fatalError("asd");
			*strchr(id, '\"') = 0;
			*strchr(name, '\"') = 0;
			iniSetValue(&config, "Twitch", "ChannelID", id);
			iniSetValue(&config, "Twitch", "Channel", name);
		}
		break;
	}

	CreateThread(0, 0, Thread1, NULL, 0, NULL);

	updateContextMenuLine(0, "fetching emotes...");

	char emoteNames[1024] = { ' ', 0 };
	AddThirdPartyEmotes(&ircsock, "api.betterttv.net", "/3/cached/emotes/global", "code\":\"", '\"', emoteNames);
	buf[0] = 0;
	str_vacat(buf, 2, "/3/cached/users/twitch/", iniGetValue(&config, "Twitch", "ChannelID"));
	AddThirdPartyEmotes(&ircsock, "api.betterttv.net", buf, "code\":\"", '\"', emoteNames);
	while (1) {
		updateContextMenuLine(0, "chat: connecting...");
		Sleep(1000);
		sslsock_disconnect(&ircsock);
		if (sslsock_connect(&ircsock, "irc.chat.twitch.tv", "6697") != 0)
			continue;
		buf[0] = 0;
		int len = str_vacat(buf, 8, "PASS oauth:", iniGetValue(&config, "Twitch", "OAuthToken"), "\r\n", "NICK ", iniGetValue(&config, "Twitch", "Channel"), "\r\nCAP REQ :twitch.tv/tags twitch.tv/commands\r\nJOIN #", iniGetValue(&config, "Twitch", "Channel"), "\r\n");
		if (sslsock_send(&ircsock, buf, len) != 0)
			continue;
		if (!(len = sslsock_recv(&ircsock, buf, 65536)))
			continue;
		buf[len] = 0;
		if (strstr(buf, ":Welcome, GLHF!") == 0)
			goto Thread2End;
		updateContextMenuLine(0, "chat: connected");
		char lastSpeakerName[32] = { 0 };
		ULONGLONG lastSpeakerTime = 0;
		char* ircMsgType = 0;
		for (int recvLen = 0; recvLen = sslsock_recv(&ircsock, buf, 65536);) {
			buf[recvLen] = 0;
			for (char* ircLineStart = buf, *ircLineEnd; ircLineEnd = strchr(ircLineStart,'\r'); ircLineStart = ircLineEnd + 2) {
				*ircLineEnd = 0;
				if (*(int*)ircLineStart == *(int*)"PING") {
					ircLineStart[1] = 'O';
					*ircLineEnd = '\r';
					sslsock_send(&ircsock, ircLineStart, (int)strlen(ircLineStart));
					continue;
				}
				if (ircMsgType = strstr((char*)ircLineStart, "PRIVMSG")) {
					char* name = rstrchr(ircLineStart, ircMsgType, '@') + 1;
					char* message = strchr(ircMsgType, ':') + 1;
					*(message - 1) = ' ';
					memcpy(ircLineEnd, " ", 2);
					*strchr(name, '.') = 0;
					//skip if necessary
					if (message[0] == '!')
						continue;
					if (iniGetValue(&config, "MutedUsers", name))
						continue;
					//strip emotes
					int emoteCount = 0;
					int allowedEmoteCount = str_getint(iniGetValue(&config, "Misc", "ReadEmotesCount"));
					char* emotes = strstr(ircLineStart, "emotes=") + 7;
					if (emotes > (char*)7) {
						*(strchr(emotes, ';') + 1) = 0;
						if (emotes[0]) {
							for (char* c = emotes; c = strchr(c + 1, '-'); emoteCount++) {
								if (emoteCount >= allowedEmoteCount) {
									char* index = c - 1;
									while (*index >= '0' && *index <= '9') index--;
									*c = 0;
									int startIndex = str_getint(index + 1);
									index = c + 1;
									while (*index >= '0' && *index <= '9') index++;
									*index = 0;
									int endIndex = str_getint(c + 1);
									memset(message + startIndex, 1, endIndex - startIndex + 1);
									c = index;
								}
							}
						}
					}
					for (char* wordStart = message, *wordEnd; wordEnd = strchr(wordStart, ' '); wordStart = wordEnd + 1) {
						char tmp = wordEnd[1];
						wordEnd[1] = 0;
						if (wordEnd - wordStart < 30 && strstr(emoteNames, wordStart - 1)) {
							emoteCount++;
							if (emoteCount > allowedEmoteCount)
								memset(wordStart, 1, wordEnd - wordStart);
						}
						wordEnd[1] = tmp;
					}
					//remove nonprintable characters
					int writePos = 0;
					for (unsigned char* c = message; *c; c++) {
						if (*c >= 32 && *c <= 126) {
							message[writePos] = *c;
							writePos++;
						}
					}
					message[writePos] = 0;
					lowercase(message);
					//select voice
					char* voice;
					char* pitchOffset = "+0Hz";
					char* userVoice = iniGetValue(&config, "UserVoices", name);
					if (userVoice) {
						char tmp[64] = { 0 };
						str_cat(tmp, userVoice);
						char* space = strchr(tmp, ' ');
						if (space) {
							*space = 0;
							pitchOffset = space + 1;
						}
						voice = tmp;
					}
					else {
						voice = iniGetValue(&config, "Misc", "DefaultVoice");
					}
					//select nickname
					char* nickname = iniGetValue(&config, "Nicknames", name);
					if (nickname)
						name = nickname;
					//choose prefix
					char msgPrefix[64] = { 0 };
					str_vacat(msgPrefix, 2, name, " says");
					ULONGLONG dt = GetTickCount64() - lastSpeakerTime;
					if (strcmp(name, lastSpeakerName) == 0 && dt < str_getint(iniGetValue(&config, "Misc", "DontRepeatNameSeconds")) * 1000)
						msgPrefix[0] = 0;
					lastSpeakerName[0] = 0;
					str_cat(lastSpeakerName, name);
					lastSpeakerTime += dt;
					//apply word replacements
					for (inikeyvalue* kv = iniGetSection(&config, "WordReplacements"); kv->key; kv++)
						strrep(message, kv->key, kv->value);
					strrep(message, "&", "&amp;");
					strrep(message, "<", "&lt;");
					//remove urls
					if (strcmp("True", iniGetValue(&config, "Misc", "BlockUrls")) == 0) {
						char* cutStart = 0;
						while ((cutStart = strstr(message, "http://")) || (cutStart = strstr(message, "https://"))) {
							memcpy(cutStart, "URL", 3);
							cutStart += 3;
							int origLen = (int)strlen(message);
							char* space = strchr(cutStart, ' ');
							char* cutEnd = space ? space : message + origLen;
							memcpy(cutStart, cutEnd, origLen - (cutEnd - message) + 1);
						}
					}
					//
					ttsAddMessage(voice, pitchOffset, 3, msgPrefix, " ", message);
				}
				if (ircMsgType = strstr((char*)ircLineStart, "USERNOTICE")) {
					if (strstr(ircLineStart, "msg-id=raid") != 0) {
						char* name = strstr(ircLineStart, "msg-param-login=") + 16;
						char* viewerCount = strstr(ircLineStart, "msg-param-viewerCount=") + 22;
						*strchr(name, ';') = 0;
						*strchr(viewerCount, ';') = 0;
						char* raidResponse = iniGetValue(&config, "Misc", "RaidChatResponse");
						if (raidResponse && *raidResponse) {
							char tmp[256] = { 0 };
							len = str_vacat(tmp, 5, "PRIVMSG #", iniGetValue(&config, "Twitch", "Channel"), " :", raidResponse, "\r\n");
							strrep(tmp, "%name", name);
							sslsock_send(&ircsock, tmp, (unsigned int)strlen(tmp));
						}
						char* raidMessage = iniGetValue(&config, "Misc", "RaidMessage");
						if (raidMessage && *raidMessage) {
							char tmp[256] = { 0 };
							str_cat(tmp, raidMessage);
							strrep(tmp, "%name", name);
							strrep(tmp, "%num", viewerCount);
							ttsAddMessage(iniGetValue(&config, "Misc", "DefaultVoice"), "+0Hz", 1, tmp);
						}
					}
					continue;
				}
			}
		}
	}
	Thread2End:
	updateContextMenuLine(0, "chat login fail");
	return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
	static HBRUSH hBrush = NULL;
	if (hBrush == NULL)
		hBrush = CreateSolidBrush(RGB(255, 200, 255));
	int ctrlId = GetDlgCtrlID((HWND)lParam);
	switch (uMsg)
	{
	case 123: {
		if (LOWORD(lParam) == WM_CONTEXTMENU) {
			hMenu = CreatePopupMenu();
			InsertMenu(hMenu, 0, MF_STRING | MF_DISABLED, 0, statusLines[0]);
			InsertMenu(hMenu, 1, MF_STRING | MF_DISABLED, 0, statusLines[1]);
			InsertMenu(hMenu, 2, MF_STRING, 1, "Config");
			InsertMenu(hMenu, 4, MF_STRING, 3, "Exit");
			SetForegroundWindow(hwnd);
			TrackPopupMenu(hMenu, 0, GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam), 0, hwnd, NULL);
			return 0;
		}
		break;
	}
	case WM_COMMAND: {
		if (wParam == 1) {
			MessageBoxA(NULL, "Note: manual edits to the caTTS.ini config file will take effect after restarting the application. For realtime changes use the chat commands!", "caTTS", 0);
			ShellExecuteA(NULL, "open", configFolder, NULL, NULL, SW_SHOW);
		}
		else if (wParam == 3) {
			PostQuitMessage(0);
		}
		break;
	}
	case WM_DESTROY: {
		PostQuitMessage(0);
		break;
	}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

#ifdef _DEBUG
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow) {
#else
void WinMainCRTStartup() {
#endif
	if (FindWindowA("caTTS", NULL) != NULL)
		fatalError("Already running");
	if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, (LPSTR)configFolder) != S_OK)
		fatalError("failed to get appdata path");
	str_cat(configFolder, "\\Catse");
	CreateDirectoryA(configFolder, NULL);
	char configPath[MAX_PATH] = { 0 };
	str_vacat(configPath,2,configFolder, "\\caTTS.ini");

	int cfgLoaded = iniParse(configPath, &config) == 0;
	if (cfgLoaded) {
		char* cfgVer = iniGetValue(&config, "Misc", "ConfigVersion");
		if (!cfgVer || *cfgVer != '1') {
			if (MessageBoxA(NULL, "An existing configuration file was found but is incompatible with this version. After pressing OK, the existing file will be renamed and a new default config will be created. You can manually transfer settings such as nicknames to the new file later.", "caTTS", MB_OKCANCEL) != IDOK) 
				ExitProcess(0);
			char oldConfigPath[MAX_PATH] = { 0 };
			str_vacat(oldConfigPath, 2, configFolder, "\\caTTS_backup.ini");
			if (MoveFileExA(configPath, oldConfigPath, MOVEFILE_REPLACE_EXISTING) == 0)
				fatalError("Failed to rename existing config file");
			free(config.data);
			cfgLoaded = 0;
		}
	}
	
	if (!cfgLoaded) {
		HANDLE hFile;
		if ((hFile = CreateFileA(configPath, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE) {
			char defaultConfig[] = {
			"[Azure]\n"
			"SubscriptionKey = 00000000000000000000000000000000\n"
			"Region = northeurope\n"
			"\n"
			"[Twitch]\n"
			"OAuthToken = 000000000000000000000000000000\n"
			"Channel = channelname\n"
			"ChannelID = 000000000\n"
			"\n"
			"[Misc]\n"
			"BlockUrls = True\n"
			"DontRepeatNameSeconds = 10\n"
			"ReadEmotesCount = 2\n"
			"DefaultVoice = en-US-AmberNeural\n"
			"EffectPlaybackDevice = devicename ;Name can be partial, for example if you have a playback device called \"FiiO BTR3K Stereo\" you can just write BTR3K. If not found, default audio output is used.\n"
			"VoicePlaybackDevice = devicename\n"
			"VoiceVolume = 100\n"
			"SoundEffectVolume = 100\n"
			"RaidChatResponse = !so %name\n"
			"RaidMessage = %name raided the channel with %num viewers\n"
			"ConfigVersion = 1\n"
			"\n"
			"[Nicknames]\n"
			"example_username = nickname\n"
			"\n"
			"[WordReplacements]\n"
			"example_word = replacement\n"
			"\n"
			"[UserVoices]\n"
			"example_username = voicename +0Hz\n"
			"\n"
			"[MutedUsers]\n"
			"example_username = True\n"
			"\n"
			"[SoundEffects]\n"
			"Reward Name = C:\\path\\to\\soundfile.wav ;only supports wav files\n"
			"\n"
			"[RedeemMessages]\n"
			"Default = %name redeemed %item for %cost points\n"
			"Reward Name = unique message\n"
			};
			DWORD numWritten;
			WriteFile(hFile, defaultConfig, (DWORD)strlen(defaultConfig), &numWritten, NULL);
			CloseHandle(hFile);
		}
		if (iniParse(configPath, &config) != 0)
			fatalError("Failed to parse config");
	}

	HINSTANCE hInstance = GetModuleHandle(0);
	WNDCLASS wc = { 0 };
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = "caTTS";
	RegisterClass(&wc);
	HWND hwnd = CreateWindowA("caTTS", "caTTS", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL,
		hInstance, NULL);
	if (hwnd == NULL)
		fatalError("failed to create window");

	NOTIFYICONDATAA iconData = { 0 };
	iconData.cbSize = sizeof(iconData);
	iconData.hWnd = hwnd;
	memcpy(iconData.szTip, "caTTS", 6);
	iconData.uFlags = NIF_TIP | NIF_ICON | NIF_MESSAGE | NIF_SHOWTIP;
	iconData.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(101));
	iconData.uCallbackMessage = 123;
	iconData.uID = 1;
	iconData.uVersion = NOTIFYICON_VERSION_4;
	if (!Shell_NotifyIconA(NIM_ADD, &iconData))
		fatalError("failed to create tray icon");
	Shell_NotifyIconA(NIM_SETVERSION, &iconData);


	for (inikeyvalue* kv = iniGetSection(&config, "Nicknames"); kv->key; kv++) lowercase(kv->key);
	for (inikeyvalue* kv = iniGetSection(&config, "WordReplacements"); kv->key; kv++) lowercase(kv->key);
	for (inikeyvalue* kv = iniGetSection(&config, "UserVoices"); kv->key; kv++) lowercase(kv->key);
	for (inikeyvalue* kv = iniGetSection(&config, "MutedUsers"); kv->key; kv++) lowercase(kv->key);


	voiceDeviceId = ds_init(iniGetValue(&config, "Misc", "VoicePlaybackDevice"));
	effectDeviceId = ds_init(iniGetValue(&config, "Misc", "EffectPlaybackDevice"));
	if (voiceDeviceId < 0 || effectDeviceId < 0)
		fatalError("failed to initialize sound devices");

	CreateThread(0, 0, Thread2, NULL, 0, NULL);


	MSG msg = { 0 };
	while (GetMessage(&msg, NULL, 0, 0) != 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	
	ExitProcess(0);
}

