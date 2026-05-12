#include <intsafe.h>
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

static const UINT Width = 1280;
static const UINT Height = 720;
static const UINT FrameCount = 2;
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
  HWND window = CreateWindowExW(0, (LPCWSTR)window_class, L"Title", window_style, CW_USEDEFAULT, CW_USEDEFAULT, Width + 16, Height + 39, 0, 0, 0, 0);
  printf("window: %llx\n", (unsigned long long)window);
  assert(window != 0);
  ShowWindow(window, SW_NORMAL);

  // Create DXGI factory
  ComPtr<IDXGIFactory4> factory;
  CreateDXGIFactory1(IID_PPV_ARGS(&factory));

  // Create device
  ComPtr<ID3D12Device> device;
  D3D12CreateDevice(
    nullptr,
    D3D_FEATURE_LEVEL_11_0,
    IID_PPV_ARGS(&device));

  // Command queue
  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

  ComPtr<ID3D12CommandQueue> queue;
  device->CreateCommandQueue(
    &queueDesc,
    IID_PPV_ARGS(&queue));

  // Swap chain (FLIP_DISCARD)
  DXGI_SWAP_CHAIN_DESC1 scDesc = {};
  scDesc.BufferCount = FrameCount;
  scDesc.Width = Width;
  scDesc.Height = Height;
  scDesc.Format = Format;
  scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  scDesc.SampleDesc.Count = 1;

  ComPtr<IDXGISwapChain1> swapChain1;
  factory->CreateSwapChainForHwnd(
    queue.Get(),
    window,
    &scDesc,
    nullptr,
    nullptr,
    &swapChain1);

  ComPtr<IDXGISwapChain3> swapChain;
  swapChain1.As(&swapChain);
  assert(swapChain.Get() == swapChain1.Get());

  // Back buffers
  UINT frameIndex = swapChain->GetCurrentBackBufferIndex();
  ComPtr<ID3D12Resource> backBuffers[FrameCount];
  for (UINT i = 0; i < FrameCount; ++i) {
    swapChain->GetBuffer(
      i,
      IID_PPV_ARGS(&backBuffers[i]));
  }

  // create GPU texture
  D3D12_HEAP_PROPERTIES defaultHeap = {.Type = D3D12_HEAP_TYPE_DEFAULT};
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
  ComPtr<ID3D12Resource> gpuTexture;
  device->CreateCommittedResource(
    &defaultHeap,
    D3D12_HEAP_FLAG_NONE,
    &gpuTextureDesc,
    D3D12_RESOURCE_STATE_COPY_DEST,
    nullptr,
    IID_PPV_ARGS(&gpuTexture));

  // Upload buffer
  UINT64 uploadSize = 0;
  device->GetCopyableFootprints(
    &gpuTextureDesc,
    0,
    1,
    0,
    nullptr,
    nullptr,
    nullptr,
    &uploadSize);
  printf("uploadSize: %llu\n", uploadSize);

  D3D12_HEAP_PROPERTIES uploadHeap = {.Type = D3D12_HEAP_TYPE_UPLOAD};
  D3D12_RESOURCE_DESC uploadDesc = {};
  uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  uploadDesc.Width = uploadSize;
  uploadDesc.Height = 1;
  uploadDesc.DepthOrArraySize = 1;
  uploadDesc.MipLevels = 1;
  uploadDesc.SampleDesc.Count = 1;
  uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  ComPtr<ID3D12Resource> uploadBuffer_gpu;
  device->CreateCommittedResource(
    &uploadHeap,
    D3D12_HEAP_FLAG_NONE,
    &uploadDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&uploadBuffer_gpu));
  uint8_t *uploadBuffer_cpu = nullptr;
  uploadBuffer_gpu->Map(0, nullptr, (void **)&uploadBuffer_cpu);

  // Command allocator/list
  ComPtr<ID3D12CommandAllocator> commandAllocator;
  device->CreateCommandAllocator(
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    IID_PPV_ARGS(&commandAllocator));

  ComPtr<ID3D12GraphicsCommandList> commandList;
  device->CreateCommandList(
    0,
    D3D12_COMMAND_LIST_TYPE_DIRECT,
    commandAllocator.Get(),
    nullptr,
    IID_PPV_ARGS(&commandList));
  commandList->Close();

  // wait for GPU
  ComPtr<ID3D12Fence> fence;
  HRESULT hr = device->CreateFence(
    0,
    D3D12_FENCE_FLAG_NONE,
    IID_PPV_ARGS(&fence));
  assert(SUCCEEDED(hr));

  HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  UINT64 fenceValue = 1;

  auto waitForGPU = [&]() {
    queue->Signal(fence.Get(), fenceValue);
    if (fence->GetCompletedValue() < fenceValue) {
      fence->SetEventOnCompletion(fenceValue, fenceEvent);
      WaitForSingleObject(fenceEvent, INFINITE);
    }
    ++fenceValue;
  };

  // CPU framebuffer (ABGR)
  uint32_t *framebuffer = new uint32_t[Width * Height];
  uint32_t t = 50;
  for (;;) {
    // inputs
    MSG message;
    while (PeekMessageW(&message, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }

    // render
    printf("t: %u", t);
    for (UINT y = 0; y < Height; ++y) {
      for (UINT x = 0; x < Width; ++x) {
        uint32_t r = (uint8_t)(x + t);
        uint32_t g = (uint8_t)y;
        uint32_t b = 0;
        framebuffer[y * Width + x] = 0xFF000000 | (b << 16) | (g << 8) | (r);
      }
    }
    t++;

    // copy framebuffer to uploadHeap
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    device->GetCopyableFootprints(
      &gpuTextureDesc,
      0,
      1,
      0,
      &footprint,
      NULL,
      NULL,
      NULL);
    printf("rowPitch: %u\n", footprint.Footprint.RowPitch);
    for (UINT y = 0; y < Height; ++y) {
      memcpy(
        uploadBuffer_cpu + y * footprint.Footprint.RowPitch,
        framebuffer + y * Width,
        Width * 4);
    }

    // copy uploadBuffer to gpuTexture
    commandAllocator->Reset();
    commandList->Reset(commandAllocator.Get(), nullptr);
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
    commandList->CopyTextureRegion(
      &dst,
      0,
      0,
      0,
      &src,
      nullptr);
    commandList->Close();

    // Execute
    ID3D12CommandList *lists[] = {commandList.Get()};
    queue->ExecuteCommandLists(1, lists);
    swapChain->Present(1, 0);
    waitForGPU();

    frameIndex = frameIndex ^ 1;
  }
  return 0;
}
