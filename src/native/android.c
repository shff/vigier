#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android_native_app_glue.h>

EGLDisplay display;
EGLSurface surface;
unsigned int gbuffer;

static void engine_handle_cmd(struct android_app *app, int32_t cmd)
{
  if (cmd == APP_CMD_INIT_WINDOW)
  {
    // Initialize Display
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, 0, 0);

    // Set Format
    const EGLint attribs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                              EGL_BLUE_SIZE,    8,
                              EGL_GREEN_SIZE,   8,
                              EGL_RED_SIZE,     8,
                              EGL_NONE};
    EGLConfig config;
    EGLint numConfigs, format;
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(app->window, 0, 0, format);
    surface = eglCreateWindowSurface(display, config, app->window, NULL);

    // Initialize OpenGL
    EGLContext context = eglCreateContext(display, config, NULL, NULL);
    eglMakeCurrent(display, surface, surface, context);

    // Get Surface Size
    EGLint width = 0, height = 0;
    eglQuerySurface(display, surface, EGL_WIDTH, &width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &height);

    // Create G-Buffer
    unsigned int backbuffer;
    glGenTextures(1, &backbuffer);
    glBindTexture(GL_TEXTURE_2D, backbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_FLOAT,
                 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    // Create Z-Buffer
    unsigned int depthbuffer;
    glGenTextures(1, &depthbuffer);
    glBindTexture(GL_TEXTURE_2D, depthbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    // Create Framebuffer
    glGenFramebuffers(1, &gbuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gbuffer);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, backbuffer, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthbuffer, 0);
    glDrawBuffers(2, (GLenum[]){GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT});

    // OpenGL Configuration
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.9, 0.9, 0.9, 1);
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
    glBindFramebuffer(GL_FRAMEBUFFER, gbuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    eglSwapBuffers(display, surface);
  }
}
