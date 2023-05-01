#include <initguid.h> 
#include <mmreg.h>
#include <dsound.h>
#pragma comment(lib, "Dsound.lib")

typedef struct {
    LPDIRECTSOUND8 lpDirectSound8;
    LPDIRECTSOUNDBUFFER8 lpDirectSoundBuffer8;
    DWORD avgBytesPerSec;
    long long bufferFormat;
    unsigned int bufferSize;
    int guidescIndex;
}devbuf;

typedef struct {
    LPGUID lpGUID;
    LPCTSTR lpszDesc;
}guidesc;

guidesc devices[32];
int deviceCount;
devbuf activeDevices[4];
int activeDeviceCount;

BOOL CALLBACK DSEnumProc(LPGUID lpGUID,LPCTSTR lpszDesc,LPCTSTR lpszDrvName,LPVOID lpContext){
    if (deviceCount < 32) {
        devices[deviceCount].lpGUID = lpGUID;
        devices[deviceCount].lpszDesc = lpszDesc;
        deviceCount++;
    }
    return TRUE;
}

// return value is negative on error, otherwise it will be a deviceId to use with ds_playsound
int ds_init(char* deviceName) {
    if (deviceCount == 0) {
        if (DirectSoundEnumerate((LPDSENUMCALLBACK)DSEnumProc, NULL) != DS_OK)
            return -1;
    }
    int guidescIndex = 0;
    for (int i = 1; i < deviceCount; i++) {
        if (strstr(devices[i].lpszDesc, deviceName) != 0) {
            guidescIndex = i;
            break;
        }
    }
    for (int i = 0; i < activeDeviceCount; i++)
        if (activeDevices[i].guidescIndex == guidescIndex)
            return i;
    if (activeDeviceCount == 4)
        return -1;
    devbuf* dev = &activeDevices[activeDeviceCount];
    activeDeviceCount++;
    dev->guidescIndex = guidescIndex;

    if (DirectSoundCreate8(devices[guidescIndex].lpGUID, &dev->lpDirectSound8, NULL) != DS_OK)
        return -1;
    if (dev->lpDirectSound8->lpVtbl->SetCooperativeLevel(dev->lpDirectSound8, GetDesktopWindow(), DSSCL_NORMAL) != DS_OK)
        return -2;
    
    return activeDeviceCount - 1;
}

int ds_playsound(int deviceId, char* filePath, char* fileData, unsigned int fileSize, int volume, int maxDurationMs) {
    devbuf* dev = &activeDevices[deviceId];

    unsigned char* fileContents = fileData;
    unsigned char* header = NULL;
    unsigned char* data = NULL;

    if (filePath) {
        if (readFileAlloc(filePath, &fileContents, &fileSize, 0) != 0)
            return -1;
    }

    header = fileContents;
    data = fileContents + 44;
    if (memcmp(&header[8], "WAVE", 4) != 0 || *(short*)&header[20] != 1)
        return -5;
    unsigned int numChannels = *(short*)&header[22];
    unsigned int sampleRate = *(int*)&header[24];
    unsigned int bitsPerSample = *(short*)&header[34];
    unsigned int dataSize = *(int*)&header[40];

    if ((fileSize - 44) < dataSize)
        return -4;

    //reuse the most recent buffer if we can
    long long bufferFormat = (long long)sampleRate + ((long long)numChannels << 32) + ((long long)bitsPerSample << 48);
    if (bufferFormat != dev->bufferFormat || dataSize > dev->bufferSize) {
        if (dev->lpDirectSoundBuffer8) {
            dev->lpDirectSoundBuffer8->lpVtbl->Release(dev->lpDirectSoundBuffer8);
            dev->lpDirectSoundBuffer8 = NULL;
        }
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
        if (dev->lpDirectSound8->lpVtbl->CreateSoundBuffer(dev->lpDirectSound8, &bufferdesc, &tmpbuffer, NULL) != DS_OK)
            return -6;
        //retrieve the directsound8 buffer from our temp buffer
        if (tmpbuffer->lpVtbl->QueryInterface(tmpbuffer, &IID_IDirectSoundBuffer8, (LPVOID*)&dev->lpDirectSoundBuffer8) != S_OK)
            return -7;
        tmpbuffer->lpVtbl->Release(tmpbuffer);
        dev->bufferFormat = bufferFormat;
        dev->bufferSize = dataSize;
        dev->avgBytesPerSec = waveformat.nAvgBytesPerSec;
    }

    //fill the secondary buffer with our WAV data
    LPVOID writePointer = NULL;
    DWORD numLockedBytes = 0;
    if (dev->lpDirectSoundBuffer8->lpVtbl->Lock(dev->lpDirectSoundBuffer8, 0, 0, &writePointer, &numLockedBytes, NULL, NULL, DSBLOCK_ENTIREBUFFER) != DS_OK)
        return -8;
    if (numLockedBytes < dataSize)
        return -9;
    memcpy(writePointer, data, dataSize);
    dev->lpDirectSoundBuffer8->lpVtbl->Unlock(dev->lpDirectSoundBuffer8, writePointer, numLockedBytes, NULL, 0);
    if (filePath)
        free(fileContents);

    dev->lpDirectSoundBuffer8->lpVtbl->SetVolume(dev->lpDirectSoundBuffer8, -10000 + volume * 100); //-10000 is lowest
    dev->lpDirectSoundBuffer8->lpVtbl->SetCurrentPosition(dev->lpDirectSoundBuffer8, 0);
    dev->lpDirectSoundBuffer8->lpVtbl->Play(dev->lpDirectSoundBuffer8, 0, 0, 0);
    DWORD playbackDurationMs = dataSize / (dev->avgBytesPerSec / 1000) - 50;
    if (playbackDurationMs > (DWORD)maxDurationMs)
        playbackDurationMs = maxDurationMs;
    Sleep(playbackDurationMs);
    dev->lpDirectSoundBuffer8->lpVtbl->Stop(dev->lpDirectSoundBuffer8);
    return 0;
}

