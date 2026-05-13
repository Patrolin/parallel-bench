// clang src/dxd12_window.cpp -o dxd12_window.exe
#include <cstdint>
#include <intsafe.h>
#include <winerror.h>
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

#define VSYNC 1
/* NOTE: must be at least 2 */
static const UINT BufferCount = 2;
/* NOTE: must be either `DXGI_FORMAT_R8G8B8A8_UNORM` or `DXGI_FORMAT_B8G8R8A8_UNORM` */
static const DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;

#pragma comment(lib, "Kernel32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D3D12.lib")

DWORD g_window_width = 0;
DWORD g_window_height = 0;
DWORD g_prev_window_width = 0;
DWORD g_prev_window_height = 0;

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
  IDXGIFactory4 *factory;
  CreateDXGIFactory1(IID_PPV_ARGS(&factory));

  // create device
  ID3D12Debug *debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    debugController->EnableDebugLayer();
  }

  ID3D12Device *device;
  D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));

  // create command queue
  D3D12_COMMAND_QUEUE_DESC queueDesc = {.Type = D3D12_COMMAND_LIST_TYPE_DIRECT};
  ID3D12CommandQueue *commandQueue;
  device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));

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
  IDXGISwapChain3 *swapChain;
  factory->CreateSwapChainForHwnd(
    commandQueue,
    window,
    &scDesc,
    NULL,
    NULL,
    (IDXGISwapChain1 **)&swapChain);

  HRESULT hr = swapChain->SetMaximumFrameLatency(1);
  assert(SUCCEEDED(hr));
  HANDLE latencyHandle = swapChain->GetFrameLatencyWaitableObject();
  assert(latencyHandle != INVALID_HANDLE_VALUE);

  // get back buffers
  UINT frameIndex = swapChain->GetCurrentBackBufferIndex();
  ID3D12Resource *backBuffers[BufferCount];
  for (UINT i = 0; i < BufferCount; ++i) {
    swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i]));
  }

  // create uploadBuffer
  ID3D12Resource *uploadBuffer_gpu = NULL;
  D3D12_RANGE *uploadBuffer_cpu = NULL;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
  D3D12_RESOURCE_DESC gpuTextureDesc;
  D3D12_RESOURCE_DESC uploadBufferDesc;
  D3D12_HEAP_PROPERTIES uploadHeap = {.Type = D3D12_HEAP_TYPE_UPLOAD};

  // create command list
  ID3D12CommandAllocator *commandAllocator;
  device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));

  ID3D12GraphicsCommandList *commandList;
  device->CreateCommandList(
    0,
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    commandAllocator,
    NULL,
    IID_PPV_ARGS(&commandList));
  commandList->Close();

  ID3D12Fence *fence;
  device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
  UINT64 fence_value = 0;
  HANDLE fence_event = CreateEvent(NULL, false, false, NULL);

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
    WaitForSingleObject(latencyHandle, INFINITE);

    // wait for gpu idle
    fence_value++;
    HRESULT hr = commandQueue->Signal(fence, fence_value);
    assert(SUCCEEDED(hr));
    if (fence->GetCompletedValue() < fence_value) {
      hr = fence->SetEventOnCompletion(fence_value, fence_event);
      assert(SUCCEEDED(hr));
      WaitForSingleObject(fence_event, INFINITE);
    }

    // resize buffers
    uint32_t *framebuffer = new uint32_t[g_window_width * g_window_height];
    if (g_window_width != g_prev_window_width && g_window_height != g_prev_window_height) {
      g_prev_window_width = g_window_width;
      g_prev_window_height = g_window_height;

      if (uploadBuffer_gpu != NULL) {
        uploadBuffer_gpu->Unmap(0, 0);
        uploadBuffer_cpu = NULL;

        uploadBuffer_gpu->Release();
        uploadBuffer_gpu = NULL;

        for (UINT i = 0; i < BufferCount; ++i) { backBuffers[i]->Release(); }
        hr = swapChain->ResizeBuffers(BufferCount, g_window_width, g_window_height, Format, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
        assert(SUCCEEDED(hr));
        for (UINT i = 0; i < BufferCount; ++i) { swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])); }
        frameIndex = swapChain->GetCurrentBackBufferIndex();
      }
      gpuTextureDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Width = g_window_width,
        .Height = g_window_height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = Format,
        .SampleDesc = {.Count = 1},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
      };
      UINT64 uploadSize = 0;
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

      uploadBufferDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Width = uploadSize,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .SampleDesc = {.Count = 1},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
      };

      hr = device->CreateCommittedResource(
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
    for (UINT y = 0; y < min(g_window_height, footprint.Footprint.Height); ++y) {
      memcpy(
        ((uint8_t *)uploadBuffer_cpu) + y * footprint.Footprint.RowPitch,
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
    commandAllocator->Reset();
    commandList->Reset(commandAllocator, NULL);
    D3D12_TEXTURE_COPY_LOCATION src = {
      .pResource = uploadBuffer_gpu,
      .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
      .PlacedFootprint = footprint,
    };
    D3D12_TEXTURE_COPY_LOCATION dst = {
      .pResource = backBuffers[frameIndex],
      .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
      .SubresourceIndex = 0,
    };
    commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

    D3D12_RESOURCE_BARRIER barrier = {
      .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
      .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
      .Transition = {
        .pResource = backBuffers[frameIndex],
        .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
        .StateAfter = D3D12_RESOURCE_STATE_PRESENT,
      }};
    commandList->ResourceBarrier(1, &barrier);

    commandList->Close();
    ID3D12CommandList *lists[] = {commandList};
    commandQueue->ExecuteCommandLists(1, lists);

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
