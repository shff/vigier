@import UIKit;
@import Foundation;
@import Metal;
@import QuartzCore;
@import AudioToolbox;

NSString *shader =
    @"#include <metal_stdlib>\n"
     "using namespace metal;"
     "vertex float4 v_main(uint idx [[vertex_id]])"
     "{"
     "    float2 pos[] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };"
     "    return float4(pos[idx].xy, 0, 1);"
     "}"
     "fragment half4 f_main("
     "    float4 in [[ position ]],"
     "    texture2d<half> albedo [[ texture(0) ]]"
     ")"
     "{"
     "    constexpr sampler Sampler(coord::pixel, filter::nearest);"
     "    return half4(albedo.sample(Sampler, in.xy).xyz, 1);"
     "}";

NSString *trisShader =
    @"#include <metal_stdlib>\n"
     "using namespace metal;"
     "vertex float4 v_main("
     "    const device packed_float3* vertex_array [[ buffer(0) ]],"
     "    unsigned int vid [[ vertex_id ]])"
     "{"
     "    return float4(vertex_array[vid], 1.0);"
     "}"
     "fragment half4 f_main()"
     "{"
     "    return half4(0, 0, 0, 1);"
     "}";

typedef struct
{
  short *data;
  int state;
  size_t position;
  size_t length;
} voice;

static OSStatus audioCallback(void *inRefCon,
                              AudioUnitRenderActionFlags *ioActionFlags,
                              const AudioTimeStamp *inTimeStamp,
                              UInt32 inBusNumber, UInt32 inNumberFrames,
                              AudioBufferList *ioData)
{
  (void)ioActionFlags;
  (void)inTimeStamp;
  (void)inBusNumber;

  voice *voices = (voice *)inRefCon;
  SInt16 *left = (SInt16 *)ioData->mBuffers[0].mData;
  SInt16 *right = (SInt16 *)ioData->mBuffers[1].mData;
  for (UInt32 frame = 0; frame < inNumberFrames; frame++)
  {
    left[frame] = right[frame] = 0;
    for (int i = 0; i < 32; i++)
    {
      if (voices[i].state == 0) continue;
      if (voices[i].position >= voices[i].length - 1) voices[i].state = 0;

      left[frame] += (voices[i].data)[voices[i].position] * 1.0f;
      right[frame] += (voices[i].data)[voices[i].position] * 1.0f;
      voices[i].position++;
    }
  }

  return 0;
}

@interface App : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@property(nonatomic, assign) id<MTLDevice> device;
@property(nonatomic, assign) id<MTLCommandQueue> queue;
@property(nonatomic, assign) CAMetalLayer *layer;
@property(nonatomic, assign) id<MTLTexture> depthTexture, albedoTexture;
@property(nonatomic, assign) id<MTLRenderPipelineState> trisShader, postShader;
@property(nonatomic, assign) MTLRenderPassDescriptor *trisPass, *postPass;
@property(nonatomic, assign) double timerCurrent, lag;
@property(nonatomic, assign) int mouseMode;
@property(nonatomic, assign) float clickX, clickY, deltaX, deltaY;
@property(nonatomic, assign) voice *voices;
@end

@implementation App
- (void)applicationDidFinishLaunching:(UIApplication *)application
{
  _voices = malloc(sizeof(voice) * 32);
  memset(_voices, 0, sizeof(voice) * 32);

  // Prevent sleeping
  [UIApplication sharedApplication].idleTimerDisabled = YES;

  // Initialize Audio
  AudioComponentDescription compDesc = {
      .componentType = kAudioUnitType_Output,
      .componentSubType = kAudioUnitSubType_GenericOutput,
      .componentManufacturer = kAudioUnitManufacturer_Apple};
  AudioStreamBasicDescription audioFormat = {
      .mSampleRate = 44100.00,
      .mFormatID = kAudioFormatLinearPCM,
      .mFormatFlags = kAudioFormatFlagIsSignedInteger |
                      kAudioFormatFlagIsPacked |
                      kAudioFormatFlagIsNonInterleaved,
      .mBitsPerChannel = 16,
      .mChannelsPerFrame = 2,
      .mFramesPerPacket = 1,
      .mBytesPerFrame = 2,
      .mBytesPerPacket = 2};

  // Initialize Audio
  AudioUnit audioUnit;
  AudioComponentInstanceNew(AudioComponentFindNext(0, &compDesc), &audioUnit);
  AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat,
                       kAudioUnitScope_Input, 0, &audioFormat,
                       sizeof(audioFormat));
  AudioUnitSetProperty(audioUnit, kAudioUnitProperty_SetRenderCallback,
                       kAudioUnitScope_Input, 0,
                       &(AURenderCallbackStruct){audioCallback, _voices},
                       sizeof(AURenderCallbackStruct));
  AudioUnitInitialize(audioUnit);
  AudioOutputUnitStart(audioUnit);

  // Initialize Metal
  _device = [MTLCreateSystemDefaultDevice() autorelease];
  _queue = [_device newCommandQueue];
  _layer = [[CAMetalLayer alloc] init];
  _layer.device = _device;

  // Final State
  _postShader = [self createShader:shader];
  _trisShader = [self createShader:trisShader];

  // Geometry Pass
  _trisPass = [[MTLRenderPassDescriptor alloc] init];
  _trisPass.colorAttachments[0].loadAction = MTLLoadActionClear;
  _trisPass.colorAttachments[0].storeAction = MTLStoreActionStore;
  _trisPass.colorAttachments[0].clearColor = MTLClearColorMake(1, 0, 0, 1);
  _trisPass.depthAttachment.clearDepth = 1.0;
  _trisPass.depthAttachment.loadAction = MTLLoadActionClear;
  _trisPass.depthAttachment.storeAction = MTLStoreActionStore;

  // Post-processing Pass
  _postPass = [[MTLRenderPassDescriptor alloc] init];
  _postPass.colorAttachments[0].loadAction = MTLLoadActionLoad;
  _postPass.colorAttachments[0].storeAction = MTLStoreActionStore;
  _postPass.depthAttachment.loadAction = MTLLoadActionLoad;


  // Create Window
  _window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  [_window setRootViewController:[[UIViewController alloc] init]];
  [[_window.rootViewController.view layer] addSublayer:_layer];
  [_window makeKeyAndVisible];

  // Initialize timer
  _timerCurrent = CACurrentMediaTime();
  _lag = 0.0;

  // Reset Deltas
  _mouseMode = 2;
  _clickX = 0.0f;
  _clickY = 0.0f;
  _deltaX = 0.0f;
  _deltaY = 0.0f;

  // Add gesture recognizers
  [_window.rootViewController.view
      addGestureRecognizer:[[UITapGestureRecognizer alloc]
                               initWithTarget:self
                                       action:@selector(onTap:)]];
  [_window.rootViewController.view
      addGestureRecognizer:[[UIPanGestureRecognizer alloc]
                               initWithTarget:self
                                       action:@selector(onDrag:)]];

  // Re-create buffers when rotating the device
  [self createBuffers];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(createBuffers)
             name:UIDeviceOrientationDidChangeNotification
           object:nil];

  // Initialize loop
  [[CADisplayLink displayLinkWithTarget:self selector:@selector(render:)]
      addToRunLoop:[NSRunLoop currentRunLoop]
           forMode:NSDefaultRunLoopMode];
}

- (void)render:(CADisplayLink *)displayLink
{
  @autoreleasepool
  {
    // Update Timer
    double timerNext = CACurrentMediaTime();
    double timerDelta = timerNext - _timerCurrent;
    _timerCurrent = timerNext;

    // Fixed updates
    for (_lag += timerDelta; _lag >= 1.0 / 60.0; _lag -= 1.0 / 60.0)
    {
    }

    // Reset Deltas
    _clickX = 0.0f;
    _clickY = 0.0f;
    _deltaX = 0.0f;
    _deltaY = 0.0f;

    // Initialize Renderer
    id<CAMetalDrawable> drawable = [_layer nextDrawable];
    id buffer = [_queue commandBuffer];

    // Geometry Pass
    _trisPass.colorAttachments[0].texture = _albedoTexture;
    _trisPass.depthAttachment.texture = _depthTexture;
    id encoder1 = [buffer renderCommandEncoderWithDescriptor:_trisPass];
    [encoder1 endEncoding];

    // Post-processing Pass
    _postPass.colorAttachments[0].texture = drawable.texture;
    _postPass.depthAttachment.texture = _depthTexture;
    id encoder2 = [buffer renderCommandEncoderWithDescriptor:_postPass];
    [encoder2 setRenderPipelineState:_postShader];
    [encoder2 setFragmentTexture:_albedoTexture atIndex:0];
    [encoder2 drawPrimitives:4 vertexStart:0 vertexCount:4];
    [encoder2 endEncoding];

    // Render
    [buffer presentDrawable:drawable];
    [buffer commit];
  }
}

- (void)onTap:(UITapGestureRecognizer *)recognizer
{
  if (_mouseMode != 1 && recognizer.state == UIGestureRecognizerStateRecognized)
  {
    _clickX = [recognizer locationInView:recognizer.view].x;
    _clickY = [recognizer locationInView:recognizer.view].y;
  }
}

- (void)onDrag:(UIPanGestureRecognizer *)recognizer
{
  if (_mouseMode != 0 && recognizer.state == UIGestureRecognizerStateRecognized)
  {
    _deltaX += [recognizer translationInView:recognizer.view].y;
    _deltaY += [recognizer translationInView:recognizer.view].y;
  }
}

- (void)createBuffers
{
  CGRect bounds = [_window frame];
  _layer.frame = bounds;

  _depthTexture = [self createTexture:MTLPixelFormatDepth32Float_Stencil8
                                    w:bounds.size.width
                                    h:bounds.size.height];
  _albedoTexture = [self createTexture:MTLPixelFormatRGBA8Unorm_sRGB
                                     w:bounds.size.width
                                     h:bounds.size.height];
}

- (id<MTLRenderPipelineState>)createShader:(NSString *)shader
{
  id library = [_device newLibraryWithSource:shader options:nil error:NULL];
  MTLRenderPipelineDescriptor *desc = [MTLRenderPipelineDescriptor new];
  desc.vertexFunction = [library newFunctionWithName:@"v_main"];
  desc.fragmentFunction = [library newFunctionWithName:@"f_main"];
  desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
  desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
  return [_device newRenderPipelineStateWithDescriptor:desc error:NULL];
}

- (id<MTLTexture>)createTexture:(MTLPixelFormat)format w:(int)w h:(int)h
{
  MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
  desc.storageMode = MTLStorageModePrivate;
  desc.usage = MTLTextureUsageRenderTarget;
  desc.width = w;
  desc.height = h;
  desc.pixelFormat = format;
  return [_device newTextureWithDescriptor:desc];
}
@end

int main(int argc, char *argv[])
{
  @autoreleasepool
  {
    return UIApplicationMain(argc, argv, nil, NSStringFromClass([App class]));
  }
}
