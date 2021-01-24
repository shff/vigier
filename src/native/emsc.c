#include <GLES2/gl2.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <webgpu/webgpu.h>

int mouseMode = 0;
double timerCurrent = 0, lag = 0, w = 0, h = 0;
float scale, clickX, clickY, mouseX, mouseY, deltaX = 0.0f, deltaY = 0.0f;

int mouseCallback(int type, const EmscriptenMouseEvent *event, void *data)
{
  if (type == EMSCRIPTEN_EVENT_MOUSEDOWN && event->button == 0)
  {
    mouseX = event->targetX * scale;
    mouseY = event->targetY * scale;
  }
  if (type == EMSCRIPTEN_EVENT_MOUSEUP && event->button == 0)
  {
    EM_ASM(document.exitPointerLock());
    clickX = event->targetX * scale;
    clickY = event->targetY * scale;
  }
  if (type == EMSCRIPTEN_EVENT_MOUSEMOVE && event->buttons == mouseMode - 1)
  {
    EM_ASM(Module['canvas'].requestPointerLock());
    deltaX = (mouseX = event->targetX * scale) - mouseX;
    deltaY = (mouseY = event->targetY * scale) - mouseY;
  }

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
