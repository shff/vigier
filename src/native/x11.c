#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <time.h>

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

  // Initialize OpenGL
  int att[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
  XVisualInfo *vi = glXChooseVisual(display, 0, att);
  GLXContext context = glXCreateContext(display, vi, 0, 1);
  glXMakeCurrent(display, window, context);

  // Start the Timer
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  uint64_t timerCurrent = (time.tv_sec * 10E8 + time.tv_nsec);
  uint64_t lag = 0.0;

  int mouseX, mouseY, mouseL, mouseR;
  while (1)
  {
    XEvent e;
    XNextEvent(display, &e);
    if (e.type == ClientMessage && e.xclient.data.l[0] == deleteWindow)
      break;

    // Mouse Cursor
    if (e.type == MotionNotify)
      mouseX = e.xmotion.x, mouseY = e.xmotion.y;
    else if (e.type == ButtonPress && e.xbutton.button == 1)
      mouseL = 1;
    else if (e.type == ButtonRelease && e.xbutton.button == 1)
      mouseL = 0;
    else if (e.type == ButtonPress && e.xbutton.button == 3)
      mouseR = 1;
    else if (e.type == ButtonRelease && e.xbutton.button == 3)
      mouseR = 0;

    // Update Timer
    clock_gettime(CLOCK_MONOTONIC, &time);
    uint64_t timerNext = (time.tv_sec * 10E8 + time.tv_nsec);
    uint64_t timerDelta = timerNext - timerCurrent;
    timerCurrent = timerNext;

    // Fixed updates
    for (lag += timerDelta; lag >= 1.0 / 60.0; lag -= 1.0 / 60.0)
    {
    }

    // Render
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glXSwapBuffers(display, window);
  }

  XUnmapWindow(display, window);
  XDestroyWindow(display, window);
  XCloseDisplay(display);
  return 0;
}
