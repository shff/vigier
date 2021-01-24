#include <d3d11.h>
#include <dsound.h>
#include <windows.h>

int mouseMode = 0;
float mouseX, mouseY;
float clickX, clickY;
float deltaX, deltaY;

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam,
                            LPARAM lParam)
{
  if (message == WM_LBUTTONDOWN)
  {
    mouseX = LOWORD(lParam);
    mouseY = HIWORD(lParam);
  }
  else if (message == WM_LBUTTONUP && deltaX + deltaY > 0.0f)
  {
    SetCursor(LoadCursorW(NULL, IDC_ARROW));
    clickX = LOWORD(lParam);
    clickY = HIWORD(lParam);
  }
  else if (message == WM_MOUSEMOVE && mouseMode != 0)
  {
    SetCursor(NULL);
    deltaX += ((mouseX = LOWORD(lParam)) - mouseX);
    deltaY += ((mouseY = HIWORD(lParam)) - mouseY);
  }
  else if (message == WM_DESTROY)
  {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(window, message, wParam, lParam);
}

int main(int argc, char const *argv[])
{
  // Disable Screensaver
  SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED);

  // Create Window
  HINSTANCE instance = GetModuleHandleW(NULL);
  RegisterClass(&(WNDCLASS){.lpfnWndProc = WindowProc,
                            .hInstance = GetModuleHandle(NULL),
                            .lpszClassName = "App"});
  HWND window = CreateWindowEx(0, "App", "App", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                               CW_USEDEFAULT, NULL, NULL, instance, NULL);
  ShowWindow(window, SW_SHOWNORMAL);

  // Create DirectSound Device
  LPDIRECTSOUND dsound;
  DirectSoundCreate(0, &dsound, 0);
  dsound->lpVtbl->SetCooperativeLevel(dsound, window, DSSCL_PRIORITY);

  // Create Primary Buffer
  DSBUFFERDESC bufferDesc1 = {.dwSize = sizeof(DSBUFFERDESC),
                              .dwFlags = DSBCAPS_PRIMARYBUFFER};
  LPDIRECTSOUNDBUFFER primaryBuffer;
  dsound->lpVtbl->CreateSoundBuffer(dsound, &bufferDesc1, &primaryBuffer, 0);
  WAVEFORMATEX format = {
      .wFormatTag = WAVE_FORMAT_PCM,
      .nChannels = 2,
      .nSamplesPerSec = 44100,
      .wBitsPerSample = 16,
      .nBlockAlign = 2 * 16 / 8,
      .nAvgBytesPerSec = 44100 * 2 * 16 / 8,
      .cbSize = 0,
  };
  primaryBuffer->lpVtbl->SetFormat(primaryBuffer, &format);

  // Create Secondary Buffer
  DSBUFFERDESC bufferDesc2 = {
      .dwSize = sizeof(DSBUFFERDESC),
      .dwBufferBytes = 1024,
      .lpwfxFormat = &format,
  };
  LPDIRECTSOUNDBUFFER secondary_buffer;
  dsound->lpVtbl->CreateSoundBuffer(dsound, &bufferDesc2, &secondary_buffer, 0);

  // Create Swap-Chain
  DXGI_SWAP_CHAIN_DESC desc = {
      .BufferCount = 1,
      .BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
      .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
      .OutputWindow = window,
      .SampleDesc.Count = 4,
      .Windowed = TRUE,
  };
  IDXGISwapChain *swapchain = NULL;
  ID3D11Device *dev = NULL;
  ID3D11DeviceContext *context = NULL;
  D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL,
                                0, D3D11_SDK_VERSION, &desc, &swapchain, &dev,
                                NULL, &context);

  // Create G-Buffer
  D3D11_TEXTURE2D_DESC gBufferTexDesc = {
      .Width = 800,
      .Height = 600,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
      .SampleDesc.Count = 1,
      .BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
  };
  D3D11_RENDER_TARGET_VIEW_DESC gBufferDesc = {
      .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
      .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
  };
  ID3D11Texture2D *gBufferTex = NULL;
  ID3D11RenderTargetView *gBuffer = NULL;
  dev->lpVtbl->CreateTexture2D(dev, &gBufferTexDesc, NULL, &gBufferTex);
  dev->lpVtbl->CreateRenderTargetView(dev, (ID3D11Resource *)gBufferTex,
                                      &gBufferDesc, &gBuffer);

  // Create Z-Buffer
  D3D11_TEXTURE2D_DESC zBufferTexDesc = {
      .Width = 800,
      .Height = 600,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
      .SampleDesc.Count = 1,
      .BindFlags = D3D11_BIND_DEPTH_STENCIL,
  };
  const D3D11_DEPTH_STENCIL_VIEW_DESC zBufferDesc = {
      .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
      .ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
  };
  ID3D11Texture2D *zBufferTex = NULL;
  ID3D11RenderTargetView *zBuffer = NULL;
  dev->lpVtbl->CreateTexture2D(dev, &zBufferTexDesc, NULL, &zBufferTex);
  dev->lpVtbl->CreateDepthStencilView(dev, (ID3D11Resource *)zBufferTex,
                                      &zBufferDesc, &zBuffer);

  // Create the Backbuffer
  ID3D11Texture2D *bufferTex = NULL;
  ID3D11RenderTargetView *buffer = NULL;
  swapchain->lpVtbl->GetBuffer(swapchain, 0, &IID_ID3D11Texture2D, &bufferTex);
  dev->lpVtbl->CreateRenderTargetView(dev, (ID3D11Resource *)bufferTex, NULL,
                                      &buffer);

  // Start the Timer
  long long timerResolution;
  long long timerCurrent;
  QueryPerformanceFrequency(&timerResolution);
  QueryPerformanceCounter(&timerCurrent);
  long long lag = 0.0;

  // Reset Deltas
  mouseX = 0.0f;
  mouseY = 0.0f;
  clickX = 0.0f;
  clickY = 0.0f;

  MSG msg = {0};
  while (msg.message != WM_QUIT)
  {
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    // Update Timer
    long long timerNext;
    QueryPerformanceCounter(&timerNext);
    long long timerDelta = (timerNext - timerCurrent) * 10E8 / timerResolution;
    timerCurrent = timerNext;

    // Fixed updates
    for (lag += timerDelta; lag >= 1.0 / 60.0; lag -= 1.0 / 60.0)
    {
    }

    // Reset Deltas
    deltaX = 0.0f;
    deltaY = 0.0f;
    clickX = 0.0f;
    clickY = 0.0f;

    // Set Viewport and Blank Colors
    RECT rect;
    GetWindowRect(window, &rect);
    D3D11_VIEWPORT viewport = {0, 0, rect.right, rect.bottom, 1, 1000};
    float blankColor[4] = {0.0f, 0.2f, 0.4f, 1.0f};

    // Geometry Pass
    context->lpVtbl->OMSetRenderTargets(context, 1, &gBuffer, &zBuffer);
    context->lpVtbl->RSSetViewports(context, 1, &viewport);
    context->lpVtbl->ClearRenderTargetView(context, gBuffer, blankColor);
    context->lpVtbl->ClearDepthStencilView(context, zBuffer, D3D11_CLEAR_DEPTH,
                                           1.0f, 0);

    // Final Pass
    context->lpVtbl->OMSetRenderTargets(context, 1, &buffer, &zBuffer);
    context->lpVtbl->RSSetViewports(context, 1, &viewport);
    swapchain->lpVtbl->Present(swapchain, 0, 0);
  }

  swapchain->lpVtbl->Release(swapchain);
  gBuffer->lpVtbl->Release(gBuffer);
  gBufferTex->lpVtbl->Release(gBufferTex);
  zBuffer->lpVtbl->Release(zBuffer);
  zBufferTex->lpVtbl->Release(zBufferTex);
  buffer->lpVtbl->Release(buffer);
  bufferTex->lpVtbl->Release(bufferTex);
  dev->lpVtbl->Release(dev);
  context->lpVtbl->Release(context);

  // Re-enable Screensaver
  SetThreadExecutionState(ES_CONTINUOUS);

  return 0;
}
