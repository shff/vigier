#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android_native_app_glue.h>

EGLDisplay display;
EGLSurface surface;
EGLint w = 0, h = 0;

static void engine_handle_cmd(struct android_app *app, int32_t cmd)
{
  if (cmd == APP_CMD_INIT_WINDOW)
  {
    EGLint numConfigs, format;
    EGLConfig config;
    EGLContext context;
    const EGLint attribs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                              EGL_BLUE_SIZE,    8,
                              EGL_GREEN_SIZE,   8,
                              EGL_RED_SIZE,     8,
                              EGL_NONE};

    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, 0, 0);
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(app->window, 0, 0, format);
    surface = eglCreateWindowSurface(display, config, app->window, NULL);
    context = eglCreateContext(display, config, NULL, NULL);
    eglMakeCurrent(display, surface, surface, context);
    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
  }
}

static int32_t engine_handle_input(struct android_app *app, AInputEvent *event)
{
  return 1;
}

void android_main(struct android_app *app)
{
  app->onAppCmd = engine_handle_cmd;
  app->onInputEvent = engine_handle_input;

  // Start the Timer
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  uint64_t timerCurrent = (time.tv_sec * 10E8 + time.tv_nsec);
  uint64_t lag = 0.0;

  int events = 0;
  struct android_poll_source *source;
  while (!app->destroyRequested)
  {
    while (ALooper_pollAll(1, 0, &events, (void **)&source) >= 0)
    {
      if (source)
        source->process(app, source);
    }

    // Update Timer
    clock_gettime(CLOCK_MONOTONIC, &time);
    uint64_t timerNext = (time.tv_sec * 10E8 + time.tv_nsec);
    uint64_t timerDelta = timerNext - timerCurrent;
    timerCurrent = timerNext;

    // Fixed updates
    for (lag += timerDelta; lag >= 1.0 / 60.0; lag -= 1.0 / 60.0)
    {
    }

    // Renderer
    glClearColor(0.9, 0.9, 0.9, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(display, surface);
  }
}
