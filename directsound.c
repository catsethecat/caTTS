#include <initguid.h> 
#include <mmreg.h>
#include <dsound.h>
#pragma comment(lib, "Dsound.lib")

typedef struct {
    LPGUID lpGUID;
    LPCTSTR lpszDesc;
}guidesc;

guidesc devices[32];
int deviceCount = 0;

BOOL CALLBACK DSEnumProc(LPGUID lpGUID,LPCTSTR lpszDesc,LPCTSTR lpszDrvName,LPVOID lpContext){
    devices[deviceCount].lpGUID = lpGUID;
    devices[deviceCount].lpszDesc = lpszDesc;
    deviceCount++;
    return TRUE;
}

int play_sound(char* filePath, char* fileData, int volume, char* deviceName) {


    LPDIRECTSOUND8 lpDirectSound8 = NULL;

    deviceCount = 0;
    if (DirectSoundEnumerate((LPDSENUMCALLBACK)DSEnumProc, NULL) != DS_OK)
        return -1;

    int deviceIndex = 0;
    for (int i = 1; i < deviceCount; i++) {
        if (strstr(devices[i].lpszDesc, deviceName) != 0) {
            deviceIndex = i;
            break;
        }
    }

    //create directsound device
    if (DirectSoundCreate8(devices[deviceIndex].lpGUID, &lpDirectSound8, NULL) != DS_OK)
        return -1;
    if (lpDirectSound8->lpVtbl->SetCooperativeLevel(lpDirectSound8, GetDesktopWindow(), DSSCL_NORMAL) != DS_OK)
        return -2;

    //load wav file
    unsigned char* fileContents = fileData;
    unsigned char* header = NULL;
    unsigned char* data = NULL;

    if (filePath) {
        HANDLE hFile = CreateFileA(filePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
            return -3;
        DWORD fsize = GetFileSize(hFile, NULL);
        DWORD numRead = 0;
        fileContents = malloc(fsize);
        if (ReadFile(hFile, fileContents, fsize, &numRead, NULL) == FALSE)
            return -4;
        CloseHandle(hFile);
    }

    header = fileContents;
    data = fileContents + 44;
    if (memcmp(&header[8], "WAVE", 4) != 0 || *(short*)&header[20] != 1)
        return -5;
    unsigned int numChannels = *(short*)&header[22];
    unsigned int sampleRate = *(int*)&header[24];
    unsigned int bitsPerSample = *(short*)&header[34];
    unsigned int dataSize = *(int*)&header[40];
    //create descriptors for our DirectSound secondary buffer
    WAVEFORMATEX waveformat;
    memset(&waveformat, 0, sizeof(WAVEFORMATEX));
    waveformat.wFormatTag = WAVE_FORMAT_PCM;
    waveformat.nChannels = numChannels;
    waveformat.nSamplesPerSec = sampleRate;
    waveformat.nAvgBytesPerSec = sampleRate * numChannels * bitsPerSample / 8;
    waveformat.nBlockAlign = numChannels * bitsPerSample / 8;
    waveformat.wBitsPerSample = bitsPerSample;
    DSBUFFERDESC bufferdesc;
    memset(&bufferdesc, 0, sizeof(DSBUFFERDESC));
    bufferdesc.dwSize = sizeof(DSBUFFERDESC);
    bufferdesc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_GLOBALFOCUS;
    bufferdesc.dwBufferBytes = dataSize;
    bufferdesc.lpwfxFormat = &waveformat;
    //create temporary secondary buffer
    LPDIRECTSOUNDBUFFER tmpbuffer = NULL;
    if (lpDirectSound8->lpVtbl->CreateSoundBuffer(lpDirectSound8, &bufferdesc, &tmpbuffer, NULL) != DS_OK)
        return -6;
    //retrieve the directsound8 buffer from our temp buffer
    LPDIRECTSOUNDBUFFER8 lpDirectSoundBuffer8 = NULL;
    if (tmpbuffer->lpVtbl->QueryInterface(tmpbuffer, &IID_IDirectSoundBuffer8, (LPVOID*)&lpDirectSoundBuffer8) != S_OK)
        return -7;
    tmpbuffer->lpVtbl->Release(tmpbuffer);
    //fill the secondary buffer with our WAV data
    LPVOID writePointer = NULL;
    DWORD numLockedBytes = 0;
    if (lpDirectSoundBuffer8->lpVtbl->Lock(lpDirectSoundBuffer8, 0, 0, &writePointer, &numLockedBytes, NULL, NULL, DSBLOCK_ENTIREBUFFER) != DS_OK)
        return -8;
    if (numLockedBytes < dataSize)
        return -9;
    memcpy(writePointer, data, dataSize);
    lpDirectSoundBuffer8->lpVtbl->Unlock(lpDirectSoundBuffer8, writePointer, numLockedBytes, NULL, 0);
    if (filePath)
        free(fileContents);

    lpDirectSoundBuffer8->lpVtbl->SetVolume(lpDirectSoundBuffer8, volume); //-10000 is lowest
    lpDirectSoundBuffer8->lpVtbl->Play(lpDirectSoundBuffer8, 0, 0, 0);
    Sleep((DWORD)(dataSize / (float)waveformat.nAvgBytesPerSec * 1000));
    lpDirectSoundBuffer8->lpVtbl->Release(lpDirectSoundBuffer8);
    lpDirectSound8->lpVtbl->Release(lpDirectSound8);
    return 0;
}