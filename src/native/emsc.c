#include <GLES2/gl2.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <webgpu/webgpu.h>

double timerCurrent = 0, lag = 0, w = 0, h = 0;
float scale = 1.0f, mouseX = 0.0f, mouseY = 0.0f;
int mouseL = 0, mouseR = 0;

int mouseCallback(int type, const EmscriptenMouseEvent *event, void *data)
{
  mouseX = event->targetX * scale;
  mouseY = event->targetY * scale;

  if (event->button == 0 && type == EMSCRIPTEN_EVENT_MOUSEDOWN)
    mouseL = 1;
  if (event->button == 0 && type == EMSCRIPTEN_EVENT_MOUSEUP)
    mouseL = 0;
  if (event->button == 2 && type == EMSCRIPTEN_EVENT_MOUSEDOWN)
    mouseR = 1;
  if (event->button == 2 && type == EMSCRIPTEN_EVENT_MOUSEUP)
    mouseR = 0;

  return 1;
}

int resizeCallback(int type, const EmscriptenMouseEvent *event, void *data)
{
  emscripten_get_element_css_size("#app", &w, &h);
  emscripten_set_canvas_element_size("#app", w, h);
}

int drawFrame()
{
  // Update Timer
  double timerNext = EM_ASM_DOUBLE({return performance.now()});
  double timerDelta = (timerNext - timerCurrent) / 10E8;
  timerCurrent = timerNext;

  // Fixed updates
  for (lag += timerDelta; lag >= 1.0 / 60.0; lag -= 1.0 / 60.0)
  {
  }

  // Renderer
  glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  return 1;
}

int main(int argc, char *argv[])
{
  timerCurrent = EM_ASM_DOUBLE({return performance.now()});

  scale = emscripten_get_device_pixel_ratio();
  emscripten_get_element_css_size("#app", &w, &h);
  emscripten_set_canvas_element_size("#app", w, h);

  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);
  int context = emscripten_webgl_create_context("#app", &attrs);
  emscripten_webgl_make_context_current(context);

  emscripten_set_mousedown_callback("#app", 0, true, mouseCallback);
  emscripten_set_mouseup_callback("#app", 0, true, mouseCallback);
  emscripten_set_mousemove_callback("#app", 0, true, mouseCallback);
  emscripten_set_mouseenter_callback("#app", 0, true, mouseCallback);
  emscripten_set_mouseleave_callback("#app", 0, true, mouseCallback);
  emscripten_set_resize_callback("#app", 0, false, resizeCallback);

  emscripten_request_animation_frame_loop(drawFrame, 0);

  return 0;
}
