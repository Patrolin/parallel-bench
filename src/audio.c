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
IXAudio2MasteringVoice *xaudioMaster;
IXAudio2SourceVoice *xaudioSource;

float square_wave(int t) {
  if (t < 0) return 0;
  return t % 100 < 50 ? 0.05f : -0.05f;
}
int main() {
  // init xaudio2
  HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
  assert(hr >= 0);
  hr = XAudio2Create(&xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
  assert(hr >= 0);
  hr = xaudio->lpVtbl->CreateMasteringVoice(
    xaudio,
    &xaudioMaster,
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
    &xaudioSource,
    (WAVEFORMATEX *)&format,
    /* NOTE: try to prevent windows from speeding up the sound if it's behind */
    XAUDIO2_VOICE_NOPITCH,
    1.0,
    NULL,
    NULL,
    NULL);
  assert(hr >= 0);
  // submit audio buffer
  // TODO: submit audio in tiny chunks for low latency
#define AUDIO_CHUNK_SIZE 48000
  float audio_buffer[AUDIO_CHUNK_SIZE * AUDIO_CHANNELS] = {};
  _Static_assert(sizeof(audio_buffer[0]) == AUDIO_BITS_PER_SAMPLE / 8, "sizeof(audio_buffer[0]) == AUDIO_BITS_PER_SAMPLE / 8");
  for (int t = 0; t < AUDIO_CHUNK_SIZE; t++) {
    float left = square_wave(t);
    float right = left;

    float mid = (left + right) * 0.5f;
    float side = left - right;

    /* NOTE: add a little bit of side channel */
#if 1
    side += 0.05f * mid;
    mid -= 0.05f * mid;
#endif

    left = mid + side * 0.5f;
    right = mid - side * 0.5f;
    if (t == 0) {
      printf("left: %f, right: %f\n", left, right);
      printf("mid: %f, side: %f\n", mid, side);
    }

    audio_buffer[2 * t] = left;
    audio_buffer[2 * t + 1] = right;
  }
  printf("      [0, 1]: %f, %f\n", audio_buffer[0], audio_buffer[1]);
  printf("    [50, 51]: %f, %f\n", audio_buffer[50], audio_buffer[51]);
  printf("  [150, 151]: %f, %f\n", audio_buffer[150], audio_buffer[151]);
  printf("  [250, 251]: %f, %f\n", audio_buffer[250], audio_buffer[251]);

  XAUDIO2_BUFFER audio_buffer_info = {
    .Flags = XAUDIO2_END_OF_STREAM,
    .AudioBytes = sizeof(audio_buffer),
    .pAudioData = (BYTE *)audio_buffer,
    .LoopCount = 0,
  };
  hr = xaudioSource->lpVtbl->SubmitSourceBuffer(xaudioSource, &audio_buffer_info, NULL);
  assert(SUCCEEDED(hr));
  xaudioSource->lpVtbl->Start(xaudioSource, 0, 0);
  assert(SUCCEEDED(hr));

  for (;;) {
    // TODO: use callback to keep generating more data
    // https://learn.microsoft.com/en-us/windows/win32/xaudio2/how-to--use-source-voice-callbacks
  }
}
