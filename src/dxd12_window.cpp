// clang src/dxd12_window.cpp -o dxd12_window.exe
#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include <cstdio>
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <winuser.h>
#include <wrl.h>
#include <stdint.h>
using Microsoft::WRL::ComPtr;

#define VSYNC 1
static const UINT Width = 1280;
static const UINT Height = 720;
static const UINT BufferCount = 2;
/* NOTE: must be either `DXGI_FORMAT_R8G8B8A8_UNORM` or `DXGI_FORMAT_B8G8R8A8_UNORM` */
static const DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;

#pragma comment(lib, "Kernel32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D3D12.lib")

LRESULT __stdcall window_proc(HWND window, UINT type, WPARAM wParam, LPARAM lParam) {
  LRESULT result = 0;
  switch (type) {
  case WM_DESTROY: {
    ExitProcess(0);
  } break;
  default: {
    result = DefWindowProcW(window, type, wParam, lParam);
  } break;
  }
  return result;
}
int main() {
  HCURSOR cursor = LoadCursorW(0, IDC_ARROW);
  WNDCLASSW window_class_options = {
    .style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
    .lpfnWndProc = window_proc,
    .hCursor = cursor,
    .lpszClassName = L"window_class",
  };
  long long window_class = RegisterClassW(&window_class_options);
  assert(window_class != 0);
  DWORD window_style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
  HWND window = CreateWindowExW(0, (LPCWSTR)window_class, L"Title", window_style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, 0, 0);
  printf("window: %llx\n", (unsigned long long)window);
  assert(window != 0);
  ShowWindow(window, SW_NORMAL);

  // create DXGI factory
  ComPtr<IDXGIFactory4> factory;
  CreateDXGIFactory1(IID_PPV_ARGS(&factory));

  // create device
  ComPtr<ID3D12Device> device;
  D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));

  // create command queue
  D3D12_COMMAND_QUEUE_DESC queueDesc = {.Type = D3D12_COMMAND_LIST_TYPE_DIRECT};
  ComPtr<ID3D12CommandQueue> queue;
  device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue));

  // create swap chain (DXGI_SWAP_EFFECT_FLIP_DISCARD)
  DXGI_SWAP_CHAIN_DESC1 scDesc = {
    .Width = Width,
    .Height = Height,
    .Format = Format,
    .SampleDesc = {.Count = 1},
    .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
    .BufferCount = BufferCount,
    .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    /* NOTE: allow tearing for windows overlays */
    .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT};
  ComPtr<IDXGISwapChain1> swapChain1;
  factory->CreateSwapChainForHwnd(
    queue.Get(),
    window,
    &scDesc,
    NULL,
    NULL,
    &swapChain1);

  ComPtr<IDXGISwapChain3> swapChain;
  swapChain1.As(&swapChain);
  assert(swapChain.Get() == swapChain1.Get());
  HRESULT hr = swapChain->SetMaximumFrameLatency(1);
  assert(SUCCEEDED(hr));
  HANDLE latencyHandle = swapChain->GetFrameLatencyWaitableObject();
  assert(latencyHandle != INVALID_HANDLE_VALUE);

  // get back buffers
  UINT frameIndex = swapChain->GetCurrentBackBufferIndex();
  ComPtr<ID3D12Resource> backBuffers[BufferCount];
  for (UINT i = 0; i < BufferCount; ++i) {
    swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i]));
  }

  // create uploadBuffer
  D3D12_RESOURCE_DESC gpuTextureDesc = {
    .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
    .Width = Width,
    .Height = Height,
    .DepthOrArraySize = 1,
    .MipLevels = 1,
    .Format = Format,
    .SampleDesc = {.Count = 1},
    .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
  };
  UINT64 uploadSize = 0;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
  device->GetCopyableFootprints(
    &gpuTextureDesc,
    0,
    1,
    0,
    &footprint,
    NULL,
    NULL,
    &uploadSize);
  printf("uploadSize: %llu\n", uploadSize);

  D3D12_HEAP_PROPERTIES uploadHeap = {.Type = D3D12_HEAP_TYPE_UPLOAD};
  D3D12_RESOURCE_DESC uploadBufferDesc = {
    .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
    .Width = uploadSize,
    .Height = 1,
    .DepthOrArraySize = 1,
    .MipLevels = 1,
    .SampleDesc = {.Count = 1},
    .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
  };

  ComPtr<ID3D12Resource> uploadBuffer_gpu;
  device->CreateCommittedResource(
    &uploadHeap,
    D3D12_HEAP_FLAG_NONE,
    &uploadBufferDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    NULL,
    IID_PPV_ARGS(&uploadBuffer_gpu));
  uint8_t *uploadBuffer_cpu = NULL;
  uploadBuffer_gpu->Map(0, NULL, (void **)&uploadBuffer_cpu);

  // create command list
  ComPtr<ID3D12CommandAllocator> commandAllocator;
  device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));

  ComPtr<ID3D12GraphicsCommandList> commandList;
  device->CreateCommandList(
    0,
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    commandAllocator.Get(),
    NULL,
    IID_PPV_ARGS(&commandList));
  commandList->Close();

  uint32_t *framebuffer = new uint32_t[Width * Height];
  uint32_t t = 50;
  for (;;) {
    // handle inputs
    MSG message;
    while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }

    // wait until render queue is empty
    WaitForSingleObject(latencyHandle, INFINITE);
#if 1
    // render to framebuffer
    for (UINT y = 0; y < Height; ++y) {
      for (UINT x = 0; x < Width; ++x) {
        uint32_t r = (uint8_t)(x + t);
        uint32_t g = (uint8_t)y;
        uint32_t b = 0;
        framebuffer[y * Width + x] = 0xFF000000 | (b << 16) | (g << 8) | (r);
      }
    }
    // copy framebuffer to uploadHeap
    for (UINT y = 0; y < Height; ++y) {
      memcpy(
        uploadBuffer_cpu + y * footprint.Footprint.RowPitch,
        framebuffer + y * Width,
        Width * 4);
    }
#else
    // render to uploadHeap
    for (UINT y = 0; y < Height; ++y) {
      for (UINT x = 0; x < Width; ++x) {
        uint32_t r = (uint8_t)(x + t);
        uint32_t g = (uint8_t)y;
        uint32_t b = 0;
        uint32_t *dest = (uint32_t *)&uploadBuffer_cpu[y * footprint.Footprint.RowPitch + x * 4];
        *dest = 0xFF000000 | (b << 16) | (g << 8) | (r);
      }
    }
#endif
    t += 5;

    // copy uploadBuffer to gpuTexture
    commandAllocator->Reset();
    commandList->Reset(commandAllocator.Get(), NULL);
    D3D12_TEXTURE_COPY_LOCATION src = {
      .pResource = uploadBuffer_gpu.Get(),
      .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
      .PlacedFootprint = footprint,
    };
    D3D12_TEXTURE_COPY_LOCATION dst = {
      .pResource = backBuffers[frameIndex].Get(),
      .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
      .SubresourceIndex = 0,
    };
    commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
    commandList->Close();
    ID3D12CommandList *lists[] = {commandList.Get()};
    queue->ExecuteCommandLists(1, lists);

    // present
    // TODO: cap framerate to slightly below monitor and disable VSYNC
    if (VSYNC) {
      swapChain->Present(1, 0);
    } else {
      swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    }
    frameIndex = frameIndex ^ 1;
  }
  return 0;
}
