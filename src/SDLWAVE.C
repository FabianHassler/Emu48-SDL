/*
 *   SdlWave.c
 *
 *   This file is part of Emu48
 *
 *   Copyright (C) 2026 Fabian Hassler
 *
 */

// Win32 waveform audio output on top of an SDL3 audio stream,
// the SDL audio thread plays the written wave headers in order
// and reports each played header with MM_WOM_DONE

#include "pch.h"
#include "Emu48.h"

typedef struct
{
	SDL_AudioStream *pStream;
	UINT            uDeviceID;
	DWORD           dwThreadId;				// receiver of MM_WOM_DONE
	PWAVEHDR        pwhHead;				// queue of written wave headers
	PWAVEHDR        pwhTail;
	DWORD           dwPosition;				// play position in head buffer
} WAVEOUT_T;

//
// SDL playback device of the waveform-audio output device ID
//
static SDL_AudioDeviceID GetSdlDevice(UINT uDeviceID)
{
	SDL_AudioDeviceID *pDevices,nDevice = 0;
	int nCount;

	if (uDeviceID == WAVE_MAPPER)			// default device
		return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;

	if ((pDevices = SDL_GetAudioPlaybackDevices(&nCount)) != NULL)
	{
		if (uDeviceID < (UINT) nCount) nDevice = pDevices[uDeviceID];
		SDL_free(pDevices);
	}
	return nDevice;
}

//
// remove played header from queue, call with locked stream
//
static VOID WaveDone(WAVEOUT_T *p)
{
	PWAVEHDR pwh = p->pwhHead;

	p->pwhHead = pwh->lpNext;
	if (p->pwhHead == NULL) p->pwhTail = NULL;
	p->dwPosition = 0;
	PostThreadMessage(p->dwThreadId,MM_WOM_DONE,(WPARAM) p,(LPARAM) pwh);
	return;
}

//
// SDL audio thread needs more data, called with locked stream
//
static void SDLCALL WaveCallback(void *pData,SDL_AudioStream *pStream,int nAdditional,int nTotal)
{
	WAVEOUT_T *p = (WAVEOUT_T *) pData;

	while (nAdditional > 0 && p->pwhHead != NULL)
	{
		PWAVEHDR pwh = p->pwhHead;
		DWORD dwBytes = __min(pwh->dwBufferLength - p->dwPosition,(DWORD) nAdditional);

		SDL_PutAudioStreamData(pStream,pwh->lpData + p->dwPosition,(int) dwBytes);
		p->dwPosition += dwBytes;
		nAdditional -= (int) dwBytes;

		if (p->dwPosition >= pwh->dwBufferLength)
			WaveDone(p);					// header completely played
	}
	return;									// missing data is played as silence
	UNREFERENCED_PARAMETER(nTotal);
}

UINT waveOutGetNumDevs(VOID)
{
	int nCount = 0;
	SDL_free(SDL_GetAudioPlaybackDevices(&nCount));
	return (UINT) nCount;
}

MMRESULT waveOutGetDevCaps(UINT_PTR uDeviceID,LPWAVEOUTCAPS pwoc,UINT cbwoc)
{
	SDL_AudioDeviceID nDevice = GetSdlDevice((UINT) uDeviceID);
	LPCTSTR szName;

	if (nDevice == 0) return MMSYSERR_BADDEVICEID;

	ZeroMemory(pwoc,cbwoc);
	if ((szName = SDL_GetAudioDeviceName(nDevice)) == NULL)
		szName = _T("Default Device");
	lstrcpyn(pwoc->szPname,szName,ARRAYSIZEOF(pwoc->szPname));
	pwoc->dwFormats = WAVE_FORMAT_4M08;		// SDL converts the format
	pwoc->wChannels = 1;
	return MMSYSERR_NOERROR;
}

MMRESULT waveOutGetID(HWAVEOUT hwo,UINT *puDeviceID)
{
	*puDeviceID = ((WAVEOUT_T *) hwo)->uDeviceID;
	return MMSYSERR_NOERROR;
}

// only the format used by Sound.c: 8-bit mono PCM, CALLBACK_THREAD
MMRESULT waveOutOpen(LPHWAVEOUT phwo,UINT uDeviceID,LPCWAVEFORMATEX pwfx,DWORD_PTR dwCallback,DWORD_PTR dwInstance,DWORD fdwOpen)
{
	SDL_AudioDeviceID nDevice;
	SDL_AudioSpec sSpec;
	WAVEOUT_T *p;

	_ASSERT(pwfx->wBitsPerSample == 8 && fdwOpen == CALLBACK_THREAD);

	if ((nDevice = GetSdlDevice(uDeviceID)) == 0)
		return MMSYSERR_BADDEVICEID;

	if ((p = (WAVEOUT_T *) calloc(1,sizeof(*p))) == NULL)
		return MMSYSERR_NOMEM;

	p->uDeviceID = uDeviceID;
	p->dwThreadId = (DWORD) dwCallback;

	sSpec.format = SDL_AUDIO_U8;
	sSpec.channels = pwfx->nChannels;
	sSpec.freq = (int) pwfx->nSamplesPerSec;
	if ((p->pStream = SDL_OpenAudioDeviceStream(nDevice,&sSpec,WaveCallback,p)) == NULL)
	{
		free(p);
		return MMSYSERR_ERROR;
	}
	SDL_ResumeAudioStreamDevice(p->pStream); // device stream starts paused
	*phwo = (HWAVEOUT) p;
	return MMSYSERR_NOERROR;
	UNREFERENCED_PARAMETER(dwInstance);
	UNREFERENCED_PARAMETER(fdwOpen);
}

MMRESULT waveOutClose(HWAVEOUT hwo)
{
	WAVEOUT_T *p = (WAVEOUT_T *) hwo;

	SDL_DestroyAudioStream(p->pStream);		// also closes the device
	free(p);
	return MMSYSERR_NOERROR;
}

MMRESULT waveOutWrite(HWAVEOUT hwo,PWAVEHDR pwh,UINT cbwh)
{
	WAVEOUT_T *p = (WAVEOUT_T *) hwo;

	pwh->lpNext = NULL;

	SDL_LockAudioStream(p->pStream);
	if (p->pwhTail == NULL)					// empty queue
		p->pwhHead = pwh;
	else
		p->pwhTail->lpNext = pwh;
	p->pwhTail = pwh;
	SDL_UnlockAudioStream(p->pStream);
	return MMSYSERR_NOERROR;
	UNREFERENCED_PARAMETER(cbwh);
}

MMRESULT waveOutReset(HWAVEOUT hwo)
{
	WAVEOUT_T *p = (WAVEOUT_T *) hwo;

	SDL_LockAudioStream(p->pStream);
	while (p->pwhHead != NULL)				// return all pending headers
		WaveDone(p);
	SDL_ClearAudioStream(p->pStream);		// drop already converted data
	SDL_UnlockAudioStream(p->pStream);
	return MMSYSERR_NOERROR;
}
