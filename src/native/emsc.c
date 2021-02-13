#include <GLES2/gl2.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <webgpu/webgpu.h>

unsigned int backbuffer, depthbuffer, gbuffer, mouseMode = 2;
double timerCurrent = 0, lag = 0, w = 0, h = 0;
float scale, clickX = 0.0f, clickY = 0.0f, deltaX = 0.0f, deltaY = 0.0f;

int mouseCallback(int type, const EmscriptenMouseEvent *event, void *data)
{
  if (type == EMSCRIPTEN_EVENT_MOUSEDOWN && event->button == 0 && mouseMode)
  {
    EM_ASM(document.querySelector("canvas").requestPointerLock());
  }
  if (type == EMSCRIPTEN_EVENT_MOUSEUP && event->button == 0 && mouseMode != 1)
  {
    EM_ASM(document.exitPointerLock());
    clickX = event->targetX * scale;
    clickY = event->targetY * scale;
  }
  if (type == EMSCRIPTEN_EVENT_MOUSEMOVE && event->buttons == mouseMode - 1)
  {
    deltaX = event->movementX;
    deltaY = event->movementY;
  }

  return 1;
}

int resizeCallback(int type, const struct EmscriptenUiEvent *event, void *data)
{
  emscripten_get_element_css_size("#canvas", &w, &h);
  emscripten_set_canvas_element_size("#canvas", w, h);

  // Resize Textures
  glBindTexture(GL_TEXTURE_2D, backbuffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_FLOAT, 0);
  glBindTexture(GL_TEXTURE_2D, depthbuffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, w, h, 0,
               GL_DEPTH_COMPONENT, GL_FLOAT, 0);

  return 0;
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

  clickX = 0.0f;
  clickY = 0.0f;
  deltaX = 0.0f;
  deltaY = 0.0f;

  // Render to G-Buffer
  glBindFramebuffer(GL_FRAMEBUFFER, gbuffer);
  glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  return 1;
}

int main(int argc, char *argv[])
{
  timerCurrent = EM_ASM_DOUBLE({return performance.now()});

  scale = emscripten_get_device_pixel_ratio();
  emscripten_get_element_css_size("#canvas", &w, &h);
  emscripten_set_canvas_element_size("#canvas", w, h);

  // Initialize Audio
  EM_ASM({
    var AudioContext = window.AudioContext || window.webkitAudioContext;
    var context = new AudioContext({ sampleRate: 44100 });
    var node = context.createScriptProcessor(2048, 0, 2);
    node.onaudioprocess = (e) => {
      for (let i = 0; i < e.outputBuffer.numberOfChannels; i++) {
        let channel = e.outputBuffer.getChannelData(i);
        for (let j = 0; j < e.outputBuffer.length; j++) {
          channel[j] = 0;
        }
      }
    };
    node.connect(context.destination);
  });

  // Initialize OpenGL
  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);
  int context = emscripten_webgl_create_context("#canvas", &attrs);
  emscripten_webgl_make_context_current(context);

  // Create G-Buffer
  glGenTextures(1, &backbuffer);
  glBindTexture(GL_TEXTURE_2D, backbuffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_FLOAT, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

  // Create Z-Buffer
  glGenTextures(1, &depthbuffer);
  glBindTexture(GL_TEXTURE_2D, depthbuffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, w, h, 0,
               GL_DEPTH_COMPONENT, GL_FLOAT, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

  // Create Framebuffer
  glGenFramebuffers(1, &gbuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, gbuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         backbuffer, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         depthbuffer, 0);
  // glDrawBuffers(2, (GLenum[]){GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT});

  emscripten_set_mousedown_callback("#canvas", 0, true, mouseCallback);
  emscripten_set_mouseup_callback("#canvas", 0, true, mouseCallback);
  emscripten_set_mousemove_callback("#canvas", 0, true, mouseCallback);
  emscripten_set_mouseenter_callback("#canvas", 0, true, mouseCallback);
  emscripten_set_mouseleave_callback("#canvas", 0, true, mouseCallback);
  emscripten_set_resize_callback("#canvas", 0, true, resizeCallback);
  emscripten_request_animation_frame_loop(drawFrame, 0);

  return 0;
}
