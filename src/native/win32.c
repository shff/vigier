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

  // Initialize DX11
  IDXGISwapChain *swapchain = NULL;
  ID3D11Device *dev = NULL;
  ID3D11DeviceContext *context = NULL;
  ID3D10Texture2D *renderTexture = NULL;
  ID3D11RenderTargetView *backbuffer = NULL;
  DXGI_SWAP_CHAIN_DESC desc = {
      .BufferCount = 1,
      .BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
      .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
      .OutputWindow = window,
      .SampleDesc.Count = 4,
      .Windowed = TRUE,
  };
  D3D11_VIEWPORT viewport = {
      .TopLeftX = 0, .TopLeftY = 0, .Width = 800, .Height = 600};
  D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL,
                                0, D3D11_SDK_VERSION, &desc, &swapchain, &dev,
                                NULL, &context);
  swapchain->lpVtbl->GetBuffer(swapchain, 0, &IID_ID3D11Texture2D,
                               &renderTexture);
  dev->lpVtbl->CreateRenderTargetView(dev, (ID3D11Resource *)renderTexture,
                                      NULL, &backbuffer);
  context->lpVtbl->OMSetRenderTargets(context, 1, &backbuffer, NULL);
  context->lpVtbl->RSSetViewports(context, 1, &viewport);

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

    // Render
    float color[4] = {0.0f, 0.2f, 0.4f, 1.0f};
    context->lpVtbl->ClearRenderTargetView(context, backbuffer, color);
    swapchain->lpVtbl->Present(swapchain, 0, 0);
  }

  swapchain->lpVtbl->Release(swapchain);
  backbuffer->lpVtbl->Release(backbuffer);
  renderTexture->lpVtbl->Release(renderTexture);
  dev->lpVtbl->Release(dev);
  context->lpVtbl->Release(context);

  return 0;
}
