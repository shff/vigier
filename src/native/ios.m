@import UIKit;
@import Foundation;
@import Metal;
@import QuartzCore;
@import AudioToolbox;

NSString *shader =
    @"#include <metal_stdlib>\n"
     "using namespace metal;"
     "vertex float4 v_simple(uint idx [[vertex_id]])"
     "{"
     "    float2 pos[] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };"
     "    return float4(pos[idx].xy, 0, 1);"
     "}"
     "fragment half4 f_simple("
     "    float4 in [[ position ]],"
     "    texture2d<half> albedo [[ texture(0) ]]"
     ")"
     "{"
     "    constexpr sampler Sampler(coord::pixel, filter::nearest);"
     "    return half4(albedo.sample(Sampler, in.xy).xyz, 1);"
     "}";

static OSStatus audioCallback(void *inRefCon,
                              AudioUnitRenderActionFlags *ioActionFlags,
                              const AudioTimeStamp *inTimeStamp,
                              UInt32 inBusNumber, UInt32 inNumberFrames,
                              AudioBufferList *ioData)
{
  (void)inRefCon;
  (void)ioActionFlags;
  (void)inTimeStamp;
  (void)inBusNumber;

  Float32 *buffer = (Float32 *)ioData->mBuffers[0].mData;
  for (UInt32 frame = 0; frame < inNumberFrames; frame++)
  {
    buffer[frame] = 0;
  }

  return 0;
}

@interface App : UIResponder <UIApplicationDelegate>
@property(nonatomic, assign) UIWindow *mainWindow;
@property(nonatomic, assign) id<MTLDevice> device;
@property(nonatomic, assign) id<MTLCommandQueue> queue;
@property(nonatomic, assign) CAMetalLayer *layer;
@property(nonatomic, assign) id<MTLTexture> depthTexture;
@property(nonatomic, assign) id<MTLTexture> albedoTexture;
@property(nonatomic, assign) id<MTLRenderPipelineState> quadState;
@property(nonatomic, assign) double timerCurrent;
@property(nonatomic, assign) double lag;
@property(nonatomic, assign) int mouseMode;
@property(nonatomic, assign) float clickX, clickY;
@property(nonatomic, assign) float deltaX, deltaY;
@end

@implementation App
- (void)applicationDidFinishLaunching:(UIApplication *)application
{
  // Initialize Audio
  AudioComponentDescription compDesc = {
      .componentType = kAudioUnitType_Output,
      .componentSubType = kAudioUnitSubType_GenericOutput,
      .componentManufacturer = kAudioUnitManufacturer_Apple};
  AudioStreamBasicDescription audioFormat = {
      .mSampleRate = 44100.00,
      .mFormatID = kAudioFormatLinearPCM,
      .mFormatFlags =
          kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved,
      .mFramesPerPacket = 1,
      .mChannelsPerFrame = 1,
      .mBitsPerChannel = 32,
      .mBytesPerPacket = 4,
      .mBytesPerFrame = 4};

  // Initialize Audio
  AudioUnit audioUnit;
  AudioComponentInstanceNew(AudioComponentFindNext(0, &compDesc), &audioUnit);
  AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat,
                       kAudioUnitScope_Input, 0, &audioFormat,
                       sizeof(audioFormat));
  AudioUnitSetProperty(audioUnit, kAudioUnitProperty_SetRenderCallback,
                       kAudioUnitScope_Input, 0,
                       &(AURenderCallbackStruct){audioCallback, NULL},
                       sizeof(AURenderCallbackStruct));
  AudioUnitInitialize(audioUnit);
  AudioOutputUnitStart(audioUnit);

  // Initialize Metal
  _device = [MTLCreateSystemDefaultDevice() autorelease];
  _queue = [_device newCommandQueue];
  _layer = [[CAMetalLayer alloc] init];
  _layer.device = _device;

  // Final State
  id library = [_device newLibraryWithSource:shader options:nil error:NULL];
  MTLRenderPipelineDescriptor *desc = [MTLRenderPipelineDescriptor new];
  desc.vertexFunction = [library newFunctionWithName:@"v_simple"];
  desc.fragmentFunction = [library newFunctionWithName:@"f_simple"];
  desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
  desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
  _quadState = [_device newRenderPipelineStateWithDescriptor:desc error:NULL];

  // Create Window
  _mainWindow = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  [_mainWindow setRootViewController:[[UIViewController alloc] init]];
  [[_mainWindow.rootViewController.view layer] addSublayer:_layer];
  [_mainWindow makeKeyAndVisible];

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
  [_mainWindow.rootViewController.view
      addGestureRecognizer:[[UITapGestureRecognizer alloc]
                               initWithTarget:self
                                       action:@selector(onTap:)]];
  [_mainWindow.rootViewController.view
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

    // Renderer
    id<CAMetalDrawable> drawable = [_layer nextDrawable];
    id buffer = [_queue commandBuffer];

    // Geometry Pass
    MTLRenderPassDescriptor *pass1 = [[MTLRenderPassDescriptor alloc] init];
    pass1.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass1.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass1.colorAttachments[0].clearColor = MTLClearColorMake(1, 0, 0, 1);
    pass1.colorAttachments[0].texture = _albedoTexture;
    pass1.depthAttachment.clearDepth = 1.0;
    pass1.depthAttachment.loadAction = MTLLoadActionClear;
    pass1.depthAttachment.storeAction = MTLStoreActionStore;
    pass1.depthAttachment.texture = _depthTexture;

    // Final Pass
    MTLRenderPassDescriptor *pass2 = [[MTLRenderPassDescriptor alloc] init];
    pass2.colorAttachments[0].loadAction = MTLLoadActionLoad;
    pass2.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass2.colorAttachments[0].texture = drawable.texture;
    pass2.depthAttachment.loadAction = MTLLoadActionLoad;
    pass2.depthAttachment.texture = _depthTexture;
    id encoder1 = [buffer renderCommandEncoderWithDescriptor:pass1];
    [encoder1 endEncoding];

    // Final Pass
    id encoder2 = [buffer renderCommandEncoderWithDescriptor:pass2];
    [encoder2 setRenderPipelineState:_quadState];
    [encoder2 setFragmentTexture:_albedoTexture atIndex:0];
    [encoder2 drawPrimitives:4 vertexStart:0 vertexCount:4];
    [encoder2 endEncoding];
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
  CGRect bounds = [_mainWindow frame];
  _layer.frame = bounds;

  _depthTexture = [self createTexture:MTLPixelFormatDepth32Float_Stencil8 w:bounds.size.width h:bounds.size.height];
  _albedoTexture = [self createTexture:MTLPixelFormatRGBA8Unorm_sRGB w:bounds.size.width h:bounds.size.height];
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
