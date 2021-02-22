#include <GL/glx.h>
#include <X11/Xlib.h>
#include <alsa/asoundlib.h>

void (*glGenFramebuffers)(GLsizei n, GLuint *framebuffers);
void (*glBindFramebuffer)(GLenum target, GLuint framebuffer);
void (*glBindFramebuffer)(GLenum target, GLuint framebuffer);
void (*glFramebufferTexture)(GLenum target, GLenum attachment, GLuint texture,
                             GLint level);
void (*glDrawBuffers)(GLsizei n, const GLenum *bufs);

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
  int eventMask =
      ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask;
  XSelectInput(display, window, eventMask);
  XMapWindow(display, window);
  Atom deleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", True);
  XSetWMProtocols(display, window, &deleteWindow, 1);

  // Fullscreen Atoms
  Atom stateAtom = XInternAtom(display, "_NET_WM_STATE", False);
  Atom fullscreenAtom = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
  int fullscreen = 0;

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

  // Initialize OpenGL Extensions
  glGenFramebuffers =
      (void (*)())glXGetProcAddressARB((const unsigned char *)"glGenFramebuffers");
  glBindFramebuffer =
      (void (*)())glXGetProcAddressARB((const unsigned char *)"glBindFramebuffer");
  glBindFramebuffer =
      (void (*)())glXGetProcAddressARB((const unsigned char *)"glBindFramebuffer");
  glFramebufferTexture = (void (*)())glXGetProcAddressARB(
      (const unsigned char *)"glFramebufferTexture");
  glDrawBuffers =
      (void (*)())glXGetProcAddressARB((const unsigned char *)"glDrawBuffers");

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

  unsigned int mouseMode = 0;
  int mouseX = 0;
  int mouseY = 0;
  int clickX = 0;
  int clickY = 0;
  unsigned int deltaX = 0;
  unsigned int deltaY = 0;

  while (1)
  {
    XEvent e;
    XNextEvent(display, &e);
    if (e.type == ClientMessage &&
        e.xclient.data.l[0] == (long int)deleteWindow)
      break;

    // Mouse Cursor
    if (e.type == MotionNotify && e.xbutton.button == mouseMode - 1)
    {
      XGrabPointer(display, window, True,
                   ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                   GrabModeAsync, GrabModeAsync, window, None, CurrentTime);
      XDefineCursor(display, window, None);
      mouseX += (deltaX = e.xmotion.x - mouseX);
      mouseX += (deltaY = e.xmotion.y - mouseY);
    }
    else if (e.type == ButtonPress && e.xbutton.button == 1)
    {
      mouseX = e.xmotion.x;
      mouseY = e.xmotion.y;
    }
    else if (e.type == ButtonRelease && e.xbutton.button == 1 &&
             deltaY + deltaY == 0.0f)
    {
      if (mouseMode != 1) XUngrabPointer(display, CurrentTime);
      clickX = e.xmotion.x;
      clickY = e.xmotion.y;
    }
    else if (e.type == KeyPress && e.xkey.keycode == 13 &&
             e.xkey.state & Mod1Mask)
    {
      fullscreen = !fullscreen;
      XSendEvent(display, root, False,
                 SubstructureNotifyMask | SubstructureRedirectMask,
                 &(XEvent){.xclient.window = window,
                           .xclient.format = 32,
                           .xclient.message_type = stateAtom,
                           .xclient.data.l[0] = fullscreen,
                           .xclient.data.l[1] = fullscreenAtom,
                           .xclient.data.l[3] = 1});
    }

    // Toggle Fullscreen
    else if (e.type == KeyPress && e.xkey.keycode == 13 &&
             e.xkey.state & Mod1Mask)
    {
      fullscreen = !fullscreen;
      XEvent event = {0};
      event.xclient.window = window;
      event.xclient.format = 32;
      event.xclient.message_type = stateAtom;
      event.xclient.data.l[0] = fullscreen;
      event.xclient.data.l[1] = fullscreenAtom;
      event.xclient.data.l[3] = 1;
      XSendEvent(display, root, False,
                 SubstructureNotifyMask | SubstructureRedirectMask, &event);
    }

    // Update Timer
    clock_gettime(CLOCK_MONOTONIC, &time);
    uint64_t timerNext = (time.tv_sec * 10E8 + time.tv_nsec);
    uint64_t timerDelta = timerNext - timerCurrent;
    timerCurrent = timerNext;

    // Periodically reset the screensaver
    xscreenLag += timerDelta;
    if (xscreenLag > 30E9)
    {
      xscreenLag = 0.0;
      XResetScreenSaver(display);
    }

    // Fixed updates
    for (lag += timerDelta; lag >= 1.0 / 60.0; lag -= 1.0 / 60.0)
    {
    }

    // Reset mouse vars
    clickX = 0;
    clickY = 0;
    deltaX = 0;
    deltaY = 0;

    (void)clickX;
    (void)clickY;

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
