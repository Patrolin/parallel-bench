#include <assert.h>
#include <stdbool.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xaudio2.h>

IXAudio2 *xaudio;
IXAudio2MasteringVoice *xaudioMasterVoice;
IXAudio2SourceVoice *xaudioSourceVoice;
XAUDIO2_BUFFER xaudioBuffer;
bool audioBusy;
int main() {
  // setup
  HRESULT result = CoInitializeEx(0, COINIT_MULTITHREADED);
  assert(result >= 0);
  result = XAudio2Create(&xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
  assert(result >= 0);
  result = IXAudio2_CreateMasteringVoice(
    xaudio,
    &xaudioMasterVoice,
    XAUDIO2_DEFAULT_CHANNELS,
    XAUDIO2_DEFAULT_SAMPLERATE,
    0,
    NULL,
    NULL,
    AudioCategory_GameMedia);
  assert(result >= 0);
  // buffer
  WAVEFORMATEXTENSIBLE format = ...; // TODO: define a format
  result = IXAudio2_CreateSourceVoice(
    xaudio,
    &xaudioSourceVoice,
    &format,
    0,
    XAUDIO2_DEFAULT_FREQ_RATIO, // TODO: ?
    NULL,
    NULL, // TODO: multiple channels
    NULL);
  assert(result >= 0);
  // TODO: submit audio in 48kHz * 0.010s chunks
  for (;;) {
    // https://learn.microsoft.com/en-us/windows/win32/xaudio2/how-to--use-source-voice-callbacks
    // TODO: write to the buffer
    // IXAudio2SourceVoice::SubmitSourceBuffer(...)
    // WaitForSingleObjectEx(voiceCallback.hBufferEndEvent, INFINITE, TRUE)
  }
}
