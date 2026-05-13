// clang src/dxd12_window.cpp -o dxd12_window.exe
#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <cstdint>
#include <intsafe.h>
#include <winerror.h>
#include <assert.h>
#include <cstdio>
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <winuser.h>
#include <wrl.h>
#include <stdint.h>

#define VSYNC 1
/* NOTE: must be at least 2 */
static const UINT BufferCount = 2;
/* NOTE: must be either `DXGI_FORMAT_R8G8B8A8_UNORM` or `DXGI_FORMAT_B8G8R8A8_UNORM` */
static const DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;

#pragma comment(lib, "Kernel32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D3D12.lib")

/* NOTE: window size must be at least `882x699` to get `Hardware (Composed): Independent Flip` */
DWORD g_window_width = 0;
DWORD g_window_height = 0;
DWORD g_prev_window_width = 0;
DWORD g_prev_window_height = 0;

typedef struct GPUFrameData GPUFrameData;
struct GPUFrameData {
  UINT64 fence_value;
  ID3D12CommandAllocator *allocator;
  ID3D12GraphicsCommandList *commandList;
  ID3D12Resource *buffer;
};
typedef struct GPUData GPUData;
struct GPUData {
  IDXGIFactory4 *factory;
  ID3D12Debug *debugController;
  ID3D12Device *device;
  ID3D12CommandQueue *commandQueue;
  IDXGISwapChain4 *swapChain;
  HANDLE latencyHandle;
  ID3D12Fence *fence;
  HANDLE fence_event;
  UINT64 global_fence_value;
  GPUFrameData frames[BufferCount];
};
GPUData g_gpu;

LRESULT __stdcall window_proc(HWND window, UINT type, WPARAM wParam, LPARAM lParam) {
  LRESULT result = 0;
  switch (type) {
  case WM_SIZE: {
    g_window_width = (DWORD)(lParam) & 0xffff;
    g_window_height = (DWORD)(lParam) >> 16;
    printf("size: %lu, %lu\n", g_window_width, g_window_height);
  } break;
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
  CreateDXGIFactory1(IID_PPV_ARGS(&g_gpu.factory));

  // create device
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&g_gpu.debugController)))) {
    g_gpu.debugController->EnableDebugLayer();
  }
  D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_gpu.device));

  // create command queue
  D3D12_COMMAND_QUEUE_DESC queueDesc = {.Type = D3D12_COMMAND_LIST_TYPE_DIRECT};
  g_gpu.device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_gpu.commandQueue));

  // create swap chain (DXGI_SWAP_EFFECT_FLIP_DISCARD)
  DXGI_SWAP_CHAIN_DESC1 scDesc = {
    .Width = g_window_width,
    .Height = g_window_height,
    .Format = Format,
    .SampleDesc = {.Count = 1},
    .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
    .BufferCount = BufferCount,
    .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    /* NOTE: allow tearing for windows overlays */
    .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT};
  g_gpu.factory->CreateSwapChainForHwnd(
    g_gpu.commandQueue,
    window,
    &scDesc,
    NULL,
    NULL,
    (IDXGISwapChain1 **)&g_gpu.swapChain);

  HRESULT hr = g_gpu.swapChain->SetMaximumFrameLatency(1);
  assert(SUCCEEDED(hr));
  g_gpu.latencyHandle = g_gpu.swapChain->GetFrameLatencyWaitableObject();
  assert(g_gpu.latencyHandle != INVALID_HANDLE_VALUE);

  // get back buffers
  UINT frameIndex = g_gpu.swapChain->GetCurrentBackBufferIndex();
  for (UINT i = 0; i < BufferCount; ++i) {
    g_gpu.swapChain->GetBuffer(i, IID_PPV_ARGS(&g_gpu.frames[i].buffer));
  }

  // create uploadBuffer
  ID3D12Resource *uploadBuffer_gpu = NULL;
  uint8_t *uploadBuffer_cpu = NULL;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};

  // create command list
  for (UINT i = 0; i < BufferCount; i++) {
    g_gpu.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_gpu.frames[i].allocator));
    g_gpu.device->CreateCommandList(
      0,
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      g_gpu.frames[i].allocator,
      NULL,
      IID_PPV_ARGS(&g_gpu.frames[i].commandList));
    g_gpu.frames[i].commandList->Close();
  };

  g_gpu.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_gpu.fence));
  g_gpu.fence_event = CreateEvent(NULL, false, false, NULL);

  // app
  uint32_t t = 50;
  for (;;) {
    // handle inputs
    MSG message;
    while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }

    // wait until render queue is empty
    WaitForSingleObject(g_gpu.latencyHandle, INFINITE);

    // wait until gpu buffer is idle
    GPUFrameData *frame = &g_gpu.frames[frameIndex];
    if (g_gpu.fence->GetCompletedValue() < frame->fence_value) {
      g_gpu.fence->SetEventOnCompletion(frame->fence_value, g_gpu.fence_event);
      WaitForSingleObject(g_gpu.fence_event, INFINITE);
    }

    // resize buffers
    uint32_t *framebuffer = new uint32_t[g_window_width * g_window_height];
    if (g_window_width != g_prev_window_width || g_window_height != g_prev_window_height) {
      g_prev_window_width = g_window_width;
      g_prev_window_height = g_window_height;

      if (uploadBuffer_gpu != NULL) {
        uploadBuffer_gpu->Unmap(0, 0);
        uploadBuffer_cpu = NULL;

        uploadBuffer_gpu->Release();
        uploadBuffer_gpu = NULL;

        for (UINT i = 0; i < BufferCount; ++i) { g_gpu.frames[i].buffer->Release(); }
        hr = g_gpu.swapChain->ResizeBuffers(BufferCount, g_window_width, g_window_height, Format, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
        assert(SUCCEEDED(hr));
        for (UINT i = 0; i < BufferCount; ++i) { g_gpu.swapChain->GetBuffer(i, IID_PPV_ARGS(&g_gpu.frames[i].buffer)); }
        frameIndex = g_gpu.swapChain->GetCurrentBackBufferIndex();
        frame = &g_gpu.frames[frameIndex];
      }
      D3D12_RESOURCE_DESC gpuTextureDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Width = g_window_width,
        .Height = g_window_height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = Format,
        .SampleDesc = {.Count = 1},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
      };
      UINT64 uploadSize = 0;
      g_gpu.device->GetCopyableFootprints(
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
      hr = g_gpu.device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        NULL,
        IID_PPV_ARGS(&uploadBuffer_gpu));
      assert(SUCCEEDED(hr));

      hr = uploadBuffer_gpu->Map(0, NULL, (void **)&uploadBuffer_cpu);
      assert(SUCCEEDED(hr));
    }

#if 1
    // render to framebuffer
    for (UINT y = 0; y < g_window_height; ++y) {
      for (UINT x = 0; x < g_window_width; ++x) {
        uint32_t r = (uint8_t)(x + t);
        uint32_t g = (uint8_t)y;
        uint32_t b = 0;
        framebuffer[y * g_window_width + x] = 0xFF000000 | (b << 16) | (g << 8) | (r);
      }
    }
    // copy framebuffer to uploadHeap
    printf("x: %llu/%llu, y: %llu/%llu\n", g_window_width, footprint.Footprint.Width, g_window_height, footprint.Footprint.Height);
    for (UINT y = 0; y < min(g_window_height, footprint.Footprint.Height); ++y) {
      memcpy(
        uploadBuffer_cpu + y * footprint.Footprint.RowPitch,
        framebuffer + y * g_window_width,
        min(g_window_width, footprint.Footprint.Width) * 4);
    }
#else
    // render to uploadHeap
    for (UINT y = 0; y < footprint.Footprint.Height; ++y) {
      for (UINT x = 0; x < footprint.Footprint.Width; ++x) {
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
    frame->allocator->Reset();
    frame->commandList->Reset(frame->allocator, NULL);

    D3D12_TEXTURE_COPY_LOCATION src = {
      .pResource = uploadBuffer_gpu,
      .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
      .PlacedFootprint = footprint,
    };
    D3D12_TEXTURE_COPY_LOCATION dst = {
      .pResource = frame->buffer,
      .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
      .SubresourceIndex = 0,
    };
    frame->commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

    D3D12_RESOURCE_BARRIER barrier = {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
      .Transition = {
        .pResource = frame->buffer,
        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
        .StateAfter = D3D12_RESOURCE_STATE_PRESENT,
      }};
    frame->commandList->ResourceBarrier(1, &barrier);

    frame->commandList->Close();
    g_gpu.commandQueue->ExecuteCommandLists(1, (ID3D12CommandList *const *)&frame->commandList);

    // present
    // TODO: cap framerate to slightly below monitor and disable VSYNC
    if (VSYNC) {
      g_gpu.swapChain->Present(1, 0);
    } else {
      g_gpu.swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    }
    frameIndex = frameIndex ^ 1;

    // signal frame fence
    g_gpu.commandQueue->Signal(g_gpu.fence, ++g_gpu.global_fence_value);
    frame->fence_value = g_gpu.global_fence_value;
  }
  return 0;
}
