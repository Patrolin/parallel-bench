#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <windows.h>
#include <xaudio2.h>
#include <errhandlingapi.h>
#include <winerror.h>

#pragma comment(lib, "Ole32.lib")

IXAudio2 *xaudio;
IXAudio2MasteringVoice *xaudioMasterVoice;
IXAudio2SourceVoice *xaudioSourceVoice;

bool audioBusy;
int main() {
  // init xaudio2
  HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
  assert(hr >= 0);
  hr = XAudio2Create(&xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
  assert(hr >= 0);
  hr = xaudio->lpVtbl->CreateMasteringVoice(
    xaudio,
    &xaudioMasterVoice,
    XAUDIO2_DEFAULT_CHANNELS,
    XAUDIO2_DEFAULT_SAMPLERATE,
    0,
    NULL,
    NULL,
    AudioCategory_GameMedia);
  assert(hr >= 0);
  // init buffers
#define SAMPLE_RATE     48000
#define CHANNELS        1
#define BITS_PER_SAMPLE 16
  uint16_t nBlockAlign = (CHANNELS * BITS_PER_SAMPLE) / 8;
  WAVEFORMATEX format = {
    .wFormatTag = WAVE_FORMAT_PCM,
    .nSamplesPerSec = SAMPLE_RATE,
    .nChannels = CHANNELS,
    .wBitsPerSample = BITS_PER_SAMPLE,
    .nBlockAlign = nBlockAlign,
    .nAvgBytesPerSec = SAMPLE_RATE * nBlockAlign,
  };
  hr = xaudio->lpVtbl->CreateSourceVoice(
    xaudio,
    &xaudioSourceVoice,
    (WAVEFORMATEX *)&format,
    0,
    XAUDIO2_DEFAULT_FREQ_RATIO, // TODO: MaxFrequencyRatio?
    NULL,
    NULL, // TODO: multiple channels?
    NULL);
  assert(hr >= 0);
// TODO: submit audio in 48kHz * 0.010s chunks
#define AUDIO_BUFFER_SIZE 48000
  int16_t audio_buffer[AUDIO_BUFFER_SIZE] = {};
  for (int i = 0; i < AUDIO_BUFFER_SIZE; i++) {
    if ((i % 100) < 50) {
      audio_buffer[i] = 1500;
    } else {
      audio_buffer[i] = -1500;
    }
  }
  printf("[0]: %i\n", audio_buffer[0]);
  printf("[50]: %i\n", audio_buffer[50]);
  printf("[100]: %i\n", audio_buffer[100]);
  XAUDIO2_BUFFER xaudioBuffer = {
    .Flags = XAUDIO2_END_OF_STREAM,
    .AudioBytes = AUDIO_BUFFER_SIZE,
    .pAudioData = (BYTE *)audio_buffer,
    .LoopCount = 0,
  };
  hr = xaudioSourceVoice->lpVtbl->SubmitSourceBuffer(xaudioSourceVoice, &xaudioBuffer, NULL);
  assert(SUCCEEDED(hr));
  xaudioSourceVoice->lpVtbl->Start(xaudioSourceVoice, 0, 0);
  assert(SUCCEEDED(hr));
  for (;;) {
    // TODO: use callback to keep generating more data
    // https://learn.microsoft.com/en-us/windows/win32/xaudio2/how-to--use-source-voice-callbacks
  }
}
