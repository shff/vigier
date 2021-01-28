#include <GL/glew.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <alsa/asoundlib.h>

int main()
{
  Display *display = XOpenDisplay(NULL);
  if (display == NULL)
  {
    printf("Cannot open display\n");
    return 1;
  }

  // Create the Window
  int screen = DefaultScreen(display);
  Window root = RootWindow(display, screen);
  Window window = XCreateSimpleWindow(display, root, 10, 10, 640, 480, 1, 0, 0);
  XSelectInput(display, window,
               ExposureMask | KeyPressMask | ButtonPressMask |
                   ButtonReleaseMask);
  XMapWindow(display, window);
  Atom deleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", True);
  XSetWMProtocols(display, window, &deleteWindow, 1);

  // Initialize ALSA
  snd_pcm_t *pcm_handle;
  snd_pcm_open(&pcm_handle, "hw:0,0", SND_PCM_STREAM_PLAYBACK,
               SND_PCM_NONBLOCK);
  long unsigned int period_size = 1024;
  unsigned int freq = 44100;
  unsigned int periods = 2;

  // Set ALSA Hardware Parameters
  int dir = 0;
  snd_pcm_hw_params_t *hw_params = NULL;
  snd_pcm_hw_params_any(pcm_handle, hw_params);
  snd_pcm_hw_params_set_access(pcm_handle, hw_params,
                               SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &freq, &dir);
  snd_pcm_hw_params_set_channels(pcm_handle, hw_params, 2);
  snd_pcm_hw_params_set_period_size_near(pcm_handle, hw_params, &period_size,
                                         &dir);
  snd_pcm_hw_params_set_periods_min(pcm_handle, hw_params, &periods, &dir);
  snd_pcm_hw_params_set_periods_first(pcm_handle, hw_params, &periods, &dir);
  snd_pcm_hw_params(pcm_handle, hw_params);

  // Set ALSA Software Parameters
  snd_pcm_sw_params_t *sw_params = NULL;
  snd_pcm_sw_params_alloca(&sw_params);
  snd_pcm_sw_params_current(pcm_handle, sw_params);
  snd_pcm_sw_params_set_avail_min(pcm_handle, sw_params, period_size);
  snd_pcm_sw_params_set_start_threshold(pcm_handle, sw_params, 1);
  snd_pcm_sw_params(pcm_handle, sw_params);

  // Initialize OpenGL
  int att[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
  XVisualInfo *vi = glXChooseVisual(display, 0, att);
  GLXContext context = glXCreateContext(display, vi, 0, 1);
  glXMakeCurrent(display, window, context);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

  // Create G-Buffer
  unsigned int backbuffer;
  glGenTextures(1, &backbuffer);
  glBindTexture(GL_TEXTURE_2D, backbuffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 800, 600, 0, GL_RGBA, GL_FLOAT, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

  // Create Z-Buffer
  unsigned int depthbuffer;
  glGenTextures(1, &depthbuffer);
  glBindTexture(GL_TEXTURE_2D, depthbuffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 800, 600, 0,
               GL_DEPTH_COMPONENT, GL_FLOAT, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

  // Create Framebuffer
  unsigned int gbuffer;
  glGenFramebuffers(1, &gbuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, gbuffer);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, backbuffer, 0);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthbuffer, 0);
  glDrawBuffers(2, (GLenum[]){GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT});

  // Start the Timer
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  uint64_t timerCurrent = (time.tv_sec * 10E8 + time.tv_nsec);
  uint64_t lag = 0.0;
  uint64_t xscreenLag = 0.0;

  int mouseX = 0;
  int mouseY = 0;
  int clickX = 0;
  int clickY = 0;
  int deltaX = 0;
  int deltaY = 0;

  while (1)
  {
    XEvent e;
    XNextEvent(display, &e);
    if (e.type == ClientMessage &&
        e.xclient.data.l[0] == (long int)deleteWindow)
      break;

    // Mouse Cursor
    if (e.type == MotionNotify && e.xbutton.button == 1)
    {
      deltaX = (mouseX = e.xmotion.x) - mouseX;
      deltaY = (mouseY = e.xmotion.y) - mouseY;
    }
    else if (e.type == ButtonPress && e.xbutton.button == 1)
    {
      mouseX = e.xmotion.x;
      mouseY = e.xmotion.y;
    }
    else if (e.type == ButtonRelease && e.xbutton.button == 1 &&
             deltaY + deltaY == 0.0f)
    {
      clickX = e.xmotion.x;
      clickY = e.xmotion.y;
    }

    // Update Timer
    clock_gettime(CLOCK_MONOTONIC, &time);
    uint64_t timerNext = (time.tv_sec * 10E8 + time.tv_nsec);
    uint64_t timerDelta = timerNext - timerCurrent;
    timerCurrent = timerNext;

    // Periodically reset the screensaver
    xscreenLag += timerDelta;
    if (xscreenLag > 30E9) {
      xscreenLag = 0.0;
      XResetScreenSaver(display);
    }

    // Fixed updates
    for (lag += timerDelta; lag >= 1.0 / 60.0; lag -= 1.0 / 60.0)
    {
    }

    // Render to G-Buffer
    glBindFramebuffer(GL_FRAMEBUFFER, gbuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glXSwapBuffers(display, window);
  }

  XUnmapWindow(display, window);
  XDestroyWindow(display, window);
  XCloseDisplay(display);
  return 0;
}
