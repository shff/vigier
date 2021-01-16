@import UIKit;
@import Foundation;
@import Metal;
@import QuartzCore;

NSString *shader =
    @"#include <metal_stdlib>\n"
     "using namespace metal;"
     "vertex float4 v_simple("
     "    uint idx [[vertex_id]])"
     "{"
     "    float2 pos[] = { {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };"
     "    return float4(pos[idx].xy, 0, 1);"
     "}"
     "fragment half4 f_simple("
     "    float4 in [[stage_in]],"
     "    texture2d<half> albedo [[ texture(0) ]]"
     ")"
     "{"
     "    constexpr sampler Sampler(coord::pixel,filter::nearest);"
     "    return half4(albedo.sample(Sampler, 0, 0).xyz, 1);"
     "}";

@interface App : UIResponder <UIApplicationDelegate>
@property(nonatomic, assign) UIWindow *mainWindow;
@property(nonatomic, assign) id<MTLDevice> device;
@property(nonatomic, assign) id<MTLCommandQueue> queue;
@property(nonatomic, assign) CAMetalLayer *layer;
@property(nonatomic, assign) MTLRenderPassDescriptor *pass1;
@property(nonatomic, assign) MTLRenderPassDescriptor *pass2;
@property(nonatomic, assign) id<MTLTexture> depthTexture;
@property(nonatomic, assign) id<MTLTexture> albedoTexture;
@property(nonatomic, assign) id<MTLRenderPipelineState> quadState;
@property(nonatomic, assign) double timerCurrent;
@property(nonatomic, assign) double lag;
@property(nonatomic, assign) CGPoint mouseClick;
@property(nonatomic, assign) float dragDeltaX;
@property(nonatomic, assign) float dragDeltaY;
@end

@implementation App
- (void)applicationDidFinishLaunching:(UIApplication *)application
{
  // Initialize Metal
  _device = [MTLCreateSystemDefaultDevice() autorelease];
  _queue = [_device newCommandQueue];
  _layer = [[CAMetalLayer alloc] init];
  _layer.device = _device;

  // Geometry Pass
  _pass1 = [[MTLRenderPassDescriptor alloc] init];
  _pass1.colorAttachments[0].loadAction = MTLLoadActionClear;
  _pass1.colorAttachments[0].storeAction = MTLStoreActionStore;
  _pass1.colorAttachments[0].clearColor = MTLClearColorMake(1, 0, 0, 1);
  _pass1.depthAttachment.clearDepth = 1.0;
  _pass1.depthAttachment.loadAction = MTLLoadActionClear;
  _pass1.depthAttachment.storeAction = MTLStoreActionStore;

  // Final Pass
  _pass2 = [[MTLRenderPassDescriptor alloc] init];
  _pass2.colorAttachments[0].loadAction = MTLLoadActionLoad;
  _pass2.colorAttachments[0].storeAction = MTLStoreActionStore;
  _pass2.depthAttachment.loadAction = MTLLoadActionLoad;

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
  _mouseClick = CGPointMake(0.0f, 0.0f);
  _dragDeltaX = 0.0f;
  _dragDeltaY = 0.0f;

  // Add gesture recognizers
  [_mainWindow.rootViewController.view
      addGestureRecognizer:[[UITapGestureRecognizer alloc]
                               initWithTarget:self
                                       action:@selector(onTap)]];
  [_mainWindow.rootViewController.view
      addGestureRecognizer:[[UIPanGestureRecognizer alloc]
                               initWithTarget:self
                                       action:@selector(onDrag)]];

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
    _mouseClick = CGPointMake(0.0f, 0.0f);
    _dragDeltaX = 0.0f;
    _dragDeltaY = 0.0f;

    // Renderer
    id<CAMetalDrawable> drawable = [_layer nextDrawable];
    _pass2.colorAttachments[0].texture = drawable.texture;
    id buffer = [_queue commandBuffer];

    // Geometry Pass
    id encoder1 = [buffer renderCommandEncoderWithDescriptor:_pass1];
    [encoder1 endEncoding];

    // Final Pass
    id encoder2 = [buffer renderCommandEncoderWithDescriptor:_pass2];
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
  if (recognizer.state == UIGestureRecognizerStateRecognized)
  {
    _mouseClick = [recognizer locationInView:recognizer.view];
  }
}

- (void)onDrag:(UITapGestureRecognizer *)recognizer
{
  if (recognizer.state == UIGestureRecognizerStateRecognized)
  {
    _dragDeltaX += [recognizer translationInView:recognizer.view].y;
    _dragDeltaY += [recognizer translationInView:recognizer.view].y;
  }
}

- (void)createBuffers
{
  CGRect bounds = [_mainWindow frame];
  _layer.frame = bounds;

  MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
  desc.storageMode = MTLStorageModePrivate;
  desc.usage = MTLTextureUsageRenderTarget;
  desc.width = bounds.size.width;
  desc.height = bounds.size.height;

  // Depth/Stencil Texture
  desc.pixelFormat = MTLPixelFormatDepth32Float_Stencil8;
  _depthTexture = [_device newTextureWithDescriptor:desc];
  _pass1.depthAttachment.texture = _depthTexture;
  _pass2.depthAttachment.texture = _depthTexture;

  // Albedo Texture
  desc.pixelFormat = MTLPixelFormatRGBA8Unorm_sRGB;
  _albedoTexture = [_device newTextureWithDescriptor:desc];
  _pass1.colorAttachments[0].texture = _albedoTexture;
}
@end

int main(int argc, char *argv[])
{
  @autoreleasepool
  {
    return UIApplicationMain(argc, argv, nil, NSStringFromClass([App class]));
  }
}
