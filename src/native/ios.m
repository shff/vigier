@import UIKit;
@import Foundation;
@import Metal;
@import QuartzCore;

@interface App : UIResponder <UIApplicationDelegate>
@property(nonatomic, assign) id<MTLCommandQueue> queue;
@property(nonatomic, assign) CAMetalLayer *layer;
@property(nonatomic, assign) double timerCurrent;
@property(nonatomic, assign) double lag;
@end

@implementation App
- (void)applicationDidFinishLaunching:(UIApplication *)application
{
  CGRect bounds = [[UIScreen mainScreen] bounds];

  // Initialize Metal
  id device = [MTLCreateSystemDefaultDevice() autorelease];
  _queue = [device newCommandQueue];
  _layer = [[CAMetalLayer alloc] init];
  _layer.device = device;
  _layer.frame = bounds;

  // Create Window
  UIWindow *window = [[UIWindow alloc] initWithFrame:bounds];
  [window setRootViewController:[[UIViewController alloc] init]];
  [[window.rootViewController.view layer] addSublayer:_layer];
  [window makeKeyAndVisible];

  // Initialize timer
  _timerCurrent = CACurrentMediaTime();
  _lag = 0.0;

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

    // Renderer
    id<CAMetalDrawable> drawable = [_layer nextDrawable];
    MTLRenderPassDescriptor *pass = [[MTLRenderPassDescriptor alloc] init];
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(1, 1, 0, 1);
    pass.colorAttachments[0].texture = drawable.texture;

    id buffer = [_queue commandBuffer];
    id encoder = [buffer renderCommandEncoderWithDescriptor:pass];
    [encoder endEncoding];
    [buffer presentDrawable:drawable];
    [buffer commit];
  }
}
@end

int main(int argc, char *argv[])
{
  @autoreleasepool
  {
    return UIApplicationMain(argc, argv, nil, NSStringFromClass([App class]));
  }
}
