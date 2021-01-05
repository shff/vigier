#define WIN32_LEAN_AND_MEAN
#include <d3d11.h>
#include <windows.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_DESTROY: PostQuitMessage(0); return 0;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow)
{
  // Create Window
  HINSTANCE instance = GetModuleHandleW(NULL);
  RegisterClass(&(WNDCLASS){.lpfnWndProc = WindowProc,
                            .hInstance = instance,
                            .lpszClassName = "App"});
  HWND window = CreateWindowEx(0, "App", "App", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                               CW_USEDEFAULT, NULL, NULL, instance, NULL);
  ShowWindow(window, SW_SHOWNORMAL);

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
  dev->lpVtbl->CreateRenderTargetView(dev, (ID3D11Resource *)zBufferTex,
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

  int done = FALSE;
  while (!done)
  {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
    {
      if (WM_QUIT == msg.message)
      {
        done = TRUE;
        continue;
      }
      else
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }

    // Mouse state
    POINT mousePos;
    GetCursorPos(&mousePos);
    ScreenToClient(window, &mousePos);
    int mouseL = GetAsyncKeyState(VK_LBUTTON) & 1;
    int mouseR = GetAsyncKeyState(VK_RBUTTON) & 1;

    // Update Timer
    long long timerNext;
    QueryPerformanceCounter(&timerNext);
    long long timerDelta = (timerNext - timerCurrent) * 10E8 / timerResolution;
    timerCurrent = timerNext;

    // Fixed updates
    for (lag += timerDelta; lag >= 1.0 / 60.0; lag -= 1.0 / 60.0)
    {
    }

    D3D11_VIEWPORT viewport = {0, 0, 800, 600, 1, 1000};
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

  return 0;
}
