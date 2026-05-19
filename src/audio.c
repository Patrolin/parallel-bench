#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <windows.h>
#include <xaudio2.h>
#include <errhandlingapi.h>
#include <winerror.h>
#include <mmreg.h>

#pragma comment(lib, "Ole32.lib")

IXAudio2 *xaudio;
IXAudio2MasteringVoice *xaudioMasterVoice;
IXAudio2SourceVoice *xaudioSourceVoice;

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
#define AUDIO_SAMPLE_RATE     48000
#define AUDIO_CHANNELS        2
#define AUDIO_FORMAT          WAVE_FORMAT_IEEE_FLOAT
#define AUDIO_BITS_PER_SAMPLE 32
  uint16_t nBlockAlign = (AUDIO_CHANNELS * AUDIO_BITS_PER_SAMPLE) / 8;
  WAVEFORMATEXTENSIBLE format = {
    .Format = {
      .wFormatTag = AUDIO_FORMAT,
      .nChannels = AUDIO_CHANNELS,
      .nSamplesPerSec = AUDIO_SAMPLE_RATE,
      .nAvgBytesPerSec = AUDIO_SAMPLE_RATE * nBlockAlign,
      .nBlockAlign = nBlockAlign,
      .wBitsPerSample = AUDIO_BITS_PER_SAMPLE,
      .cbSize = 0,
    },
    .dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT,
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
#define AUDIO_CHUNK_SIZE   48000
#define LEFT_CHANNEL_DELAY 1
  float audio_buffer[AUDIO_CHUNK_SIZE * AUDIO_CHANNELS] = {};
  _Static_assert(sizeof(audio_buffer[0]) == AUDIO_BITS_PER_SAMPLE / 8, "sizeof(audio_buffer[0]) == AUDIO_BITS_PER_SAMPLE / 8");
  for (int t = 0; t < AUDIO_CHUNK_SIZE; t++) {
    float left = (t - LEFT_CHANNEL_DELAY) % 100 < 50 ? 0.05f : -0.05f;
    if (t - LEFT_CHANNEL_DELAY < 0) left = 0;
    float right = t % 100 < 50 ? 0.05f : -0.05f;
    audio_buffer[2 * t] = left;
    audio_buffer[2 * t + 1] = right;
  }
  printf("      [0, 1]: %i, %i\n", audio_buffer[0], audio_buffer[1]);
  printf("    [50, 51]: %i, %i\n", audio_buffer[50], audio_buffer[51]);
  printf("  [150, 151]: %i, %i\n", audio_buffer[150], audio_buffer[151]);
  printf("  [250, 251]: %i, %i\n", audio_buffer[250], audio_buffer[251]);
  XAUDIO2_BUFFER xaudioBuffer = {
    .Flags = XAUDIO2_END_OF_STREAM,
    .AudioBytes = AUDIO_CHUNK_SIZE * sizeof(audio_buffer[0]),
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
