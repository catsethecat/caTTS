#define WIN32_LEAN_AND_MEAN
#define TTS_QUEUE_COUNT 10
#define TTS_MAX_MESSAGE_LEN 1024

#include <windows.h>
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
char configPath[MAX_PATH];
HWND label1;
HWND label2;
HANDLE hThread0;
HANDLE hThread1;
HANDLE hThread2;
sslsocket ircsock;
sslsocket pubsock;
inifile config;
inifile bspConfig;

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
			iniGetValue(&config, "Misc", "VoicePlaybackDevice"));

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

DWORD WINAPI Thread0(LPVOID lpParam) {
	while (1) {
		Sleep(60000);
		if (sslsock_send(&pubsock, "\x81\xfe\x0\x10\x0\x0\x0\x0{\"type\": \"PING\"}", 24) != 0)
			return 0;
	}
	return 0;
}

DWORD WINAPI Thread1(LPVOID lpParam) {
	SetWindowText(label2, "redeems: connecting          ");
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
	char* pass = iniGetValue(&config, "Twitch", "OAuthToken") + 6;
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
				buf[0] = 0;
				str_vacat(buf, 2, "redeems: ", err);
				SetWindowText(label2, buf);
				return 0;
			}
			SetWindowText(label2, "redeems: listening           ");
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
				play_sound(soundPath, NULL, str_getint(iniGetValue(&config, "Misc", "SoundEffectVolume")), iniGetValue(&config, "Misc", "EffectPlaybackDevice"));
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
	SetWindowText(label2, "redeems: stopped              ");
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
	SetWindowText(label1, "   chat: init                ");
	unsigned char buf[65536] = { 0 };
	char emoteNames[1024] = { ' ', 0 };
	AddThirdPartyEmotes(&ircsock, "api.betterttv.net", "/3/cached/emotes/global", "code\":\"", '\"', emoteNames);
	str_vacat(buf, 2, "/3/cached/users/twitch/", iniGetValue(&config, "Twitch", "ChannelID"));
	AddThirdPartyEmotes(&ircsock, "api.betterttv.net", buf, "code\":\"", '\"', emoteNames);
	while (1) {
		SetWindowText(label1, "   chat: connecting                ");
		Sleep(1000);
		sslsock_disconnect(&ircsock);
		if (sslsock_connect(&ircsock, "irc.chat.twitch.tv", "6697") != 0)
			continue;
		buf[0] = 0;
		int len = str_vacat(buf, 8, "PASS ", iniGetValue(&config, "Twitch", "OAuthToken"), "\r\n", "NICK ", iniGetValue(&config, "Twitch", "Channel"), "\r\nCAP REQ :twitch.tv/tags twitch.tv/commands\r\nJOIN #", iniGetValue(&config, "Twitch", "Channel"), "\r\n");
		if (sslsock_send(&ircsock, buf, len) != 0)
			continue;
		if (!(len = sslsock_recv(&ircsock, buf, 65536)))
			continue;
		buf[len] = 0;
		if (strstr(buf,":Welcome, GLHF!") == 0)
			goto Thread2End;
		SetWindowText(label1, "   chat: listening              ");
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
	SetWindowText(label1, "   chat: login fail            ");
	return 0;
}

void Reload() {
	//load config
	free(config.data);
	config = iniParse(configPath);
	if (config.data == 0)
		fatalError("failed to parse config");
	free(bspConfig.data);
	bspConfig = iniParse(iniGetValue(&config, "Twitch", "BSPlusConfig"));
	if (bspConfig.data) {
		int iRes = iniSetValue(&config, "Twitch", "OAuthToken", iniGetValue(&bspConfig, "Twitch", "Twitch.OAuthToken"));
		iRes = iniSetValue(&config, "Twitch", "Channel", iniGetValue(&bspConfig, "Twitch", "Twitch.Channels"));
	}
	lowercase(iniGetValue(&config, "Twitch", "Channel"));
	for (inikeyvalue* kv = iniGetSection(&config, "Nicknames"); kv->key; kv++) lowercase(kv->key);
	for (inikeyvalue* kv = iniGetSection(&config, "WordReplacements"); kv->key; kv++) lowercase(kv->key);
	for (inikeyvalue* kv = iniGetSection(&config, "UserVoices"); kv->key; kv++) lowercase(kv->key);
	for (inikeyvalue* kv = iniGetSection(&config, "MutedUsers"); kv->key; kv++) lowercase(kv->key);
	//clear tts buffer
	for (int i = 0; i < TTS_QUEUE_COUNT; i++)
		ttsQueue[i].message[0] = 0;
	tts_last_message_pos = 0;
	tts_playback_pos = 0;
	//restart threads
	if ((hThread0 != 0 && TerminateThread(hThread0, 0) == 0) ||
		(hThread1 != 0 && TerminateThread(hThread1, 0) == 0) ||
		(hThread2 != 0 && TerminateThread(hThread2, 0) == 0))
		fatalError("failed to terminate thread");
	hThread0 = hThread1 = hThread2 = 0;
	hThread1 = CreateThread(0, 0, Thread1, NULL, 0, NULL);
	hThread2 = CreateThread(0, 0, Thread2, NULL, 0, NULL);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
	static HBRUSH hBrush = NULL;
	if (hBrush == NULL)
		hBrush = CreateSolidBrush(RGB(255, 200, 255));
	int ctrlId = GetDlgCtrlID((HWND)lParam);
	switch (uMsg)
	{
	case WM_COMMAND: {
		if (HIWORD(wParam) == BN_CLICKED) {
			if (ctrlId == 10)
				ShellExecuteA(NULL, "open", configPath, NULL, NULL, SW_SHOW);
			if (ctrlId == 11)
				Reload();
		}

		break;
	}
	case WM_CTLCOLORSTATIC: {
		SetTextColor((HDC)wParam, RGB(255, 255, 255));
		SetBkColor((HDC)wParam, RGB(145, 70, 255));
		return (LRESULT)hBrush;
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
	if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, (LPSTR)configPath) != S_OK)
		fatalError("failed to get appdata path");
	str_cat(configPath, "\\Catse");
	CreateDirectoryA(configPath, NULL);
	str_cat(configPath, "\\caTTS.ini");
	HANDLE hFile;
	if ((hFile = CreateFileA(configPath, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE) {
		char defaultConfig[] = {
		"[Azure]\n"
		"SubscriptionKey = 00000000000000000000000000000000\n"
		"Region = northeurope\n"
		"\n"
		"[Twitch]\n"
		"OAuthToken = oauth:000000000000000000000000000000\n"
		"Channel = channel_name\n"
		"ChannelID = 123456789\n"
		"BSPlusConfig = C:\\Users\\Example User\\AppData\\Local\\.beatsaberpluschatcore\\auth.ini\n"
		"\n"
		"[Misc]\n"
		"BlockUrls = True\n"
		"DontRepeatNameSeconds = 10\n"
		"ReadEmotesCount = 2\n"
		"DefaultVoice = en-US-AmberNeural\n"
		"EffectPlaybackDevice = devicename ;Name can be partial, for example if you have a playback device called \"FiiO BTR3K Stereo\" you can just write BTR3K. If not found, default audio output is used.\n"
		"VoicePlaybackDevice = devicename\n"
		"VoiceVolume = 0 ;Volumes are a number between -10000 and 0\n"
		"SoundEffectVolume = 0\n"
		"RaidChatResponse = !so %name\n"
		"RaidMessage = %name raided the channel with %num viewers\n"
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
		"Reward Name = path\\to\\soundfile.wav ;only supports wav files\n"
		"\n"
		"[RedeemMessages]\n"
		"Default = %name redeemed %item for %cost points\n"
		"Reward Name = unique message\n"
		};
		DWORD numWritten;
		WriteFile(hFile, defaultConfig, (DWORD)strlen(defaultConfig), &numWritten, NULL);
		CloseHandle(hFile);
	}

	HINSTANCE hInstance = GetModuleHandle(0);
	WNDCLASS wc = { 0 };
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = "catts_class";
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(101));
	wc.hbrBackground = CreateSolidBrush(RGB(145, 70, 255));
	RegisterClass(&wc);
	HWND hwnd = CreateWindow("catts_class", "caTTS", WS_BORDER | WS_SYSMENU | WS_VISIBLE | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 288, 98, NULL, NULL, hInstance, NULL);
	if (hwnd == NULL)
		fatalError("failed to create window");

	label1 = CreateWindow("static", "   chat: null", WS_CHILD | WS_VISIBLE | SS_SIMPLE, 5, 10, 190, 16, hwnd, (HMENU)123, hInstance, NULL);
	label2 = CreateWindow("static", "redeems: null", WS_CHILD | WS_VISIBLE | SS_SIMPLE, 5, 10 + 24, 190, 16, hwnd, (HMENU)124, hInstance, NULL);
	HWND button1 = CreateWindow("BUTTON", "Config", WS_CHILD | WS_VISIBLE | BS_FLAT, 200, 8, 64, 20, hwnd, (HMENU)10, hInstance, NULL);
	HWND button2 = CreateWindow("BUTTON", "Reload", WS_CHILD | WS_VISIBLE | BS_FLAT, 200, 32, 64, 20, hwnd, (HMENU)11, hInstance, NULL);

	SendMessageA(label1, WM_SETFONT, (WPARAM)GetStockObject(SYSTEM_FIXED_FONT), TRUE);
	SendMessageA(label2, WM_SETFONT, (WPARAM)GetStockObject(SYSTEM_FIXED_FONT), TRUE);

	Reload();

	MSG msg = { 0 };
	while (GetMessage(&msg, NULL, 0, 0) != 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	
	ExitProcess(0);
}

