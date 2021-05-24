@import Cocoa;
@import IOKit.pwr_mgt;
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

@interface App : NSResponder <NSApplicationDelegate, NSWindowDelegate>
@property(nonatomic, assign) NSWindow *window;
@property(nonatomic, assign) id<MTLDevice> device;
@property(nonatomic, assign) id<MTLCommandQueue> queue;
@property(nonatomic, assign) CAMetalLayer *layer;
@property(nonatomic, assign) id<MTLTexture> depthTexture;
@property(nonatomic, assign) id<MTLTexture> albedoTexture;
@property(nonatomic, assign) id<MTLRenderPipelineState> quadState;
@property(nonatomic, assign) double timerCurrent;
@property(nonatomic, assign) double lag;
@property(nonatomic, assign) float clickX, clickY;
@property(nonatomic, assign) float deltaX, deltaY;
@property(nonatomic, assign) int mouseMode;
@property(nonatomic, assign) int cursorVisible;
@end

@implementation App
- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
  (void)notification;
  @autoreleasepool
  {
    AudioComponentDescription compDesc = {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple};
    AudioStreamBasicDescription audioFormat = {
        .mSampleRate = 44100.00,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagsNativeFloatPacked |
                        kAudioFormatFlagIsNonInterleaved,
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

    // Create the Metal device
    _device = [MTLCreateSystemDefaultDevice() autorelease];
    _queue = [_device newCommandQueue];
    _layer = [CAMetalLayer layer];
    [_layer setDevice:_device];

    _quadState = [self createState:shader];

    // Create the Window
    _window =
        [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 480)
                                    styleMask:NSWindowStyleMaskTitled |
                                              NSWindowStyleMaskResizable |
                                              NSWindowStyleMaskClosable |
                                              NSWindowStyleMaskMiniaturizable
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    [_window cascadeTopLeftFromPoint:NSMakePoint(20, 20)];
    [_window setMinSize:NSMakeSize(300, 200)];
    [_window setAcceptsMouseMovedEvents:YES];
    [_window.contentView setLayer:_layer];
    [_window makeKeyAndOrderFront:nil];
    [_window setNextResponder:self];
    [_window setDelegate:self];
    [_window center];

    // Re-create buffers when resizing windows
    [self createBuffers];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(createBuffers)
               name:NSWindowDidResizeNotification
             object:nil];

    // Disable tabbing
    if ([_window respondsToSelector:@selector(setTabbingMode:)])
      [_window setTabbingMode:NSWindowTabbingModeDisallowed];

    // Initialize timer
    _timerCurrent = CACurrentMediaTime();
    _lag = 0.0;

    // Reset Deltas
    _mouseMode = 2;
    _clickX = 0.0f;
    _clickY = 0.0f;
    _deltaX = 0.0f;
    _deltaY = 0.0f;

    // Initialize loop
    [NSTimer scheduledTimerWithTimeInterval:1.0 / 60.0
                                     target:self
                                   selector:@selector(drawFrame)
                                   userInfo:nil
                                    repeats:YES];

    [NSApp finishLaunching];
  }
}

- (void)drawFrame
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
    pass1.colorAttachments[0].texture = _albedoTexture;
    pass1.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass1.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass1.colorAttachments[0].clearColor = MTLClearColorMake(1, 0, 0, 1);
    pass1.depthAttachment.texture = _depthTexture;
    pass1.depthAttachment.clearDepth = 1.0;
    pass1.depthAttachment.loadAction = MTLLoadActionClear;
    pass1.depthAttachment.storeAction = MTLStoreActionStore;
    id encoder1 = [buffer renderCommandEncoderWithDescriptor:pass1];
    [encoder1 endEncoding];

    // Final Pass
    MTLRenderPassDescriptor *pass2 = [[MTLRenderPassDescriptor alloc] init];
    pass2.colorAttachments[0].texture = drawable.texture;
    pass2.colorAttachments[0].loadAction = MTLLoadActionLoad;
    pass2.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass2.depthAttachment.texture = _depthTexture;
    pass2.depthAttachment.loadAction = MTLLoadActionLoad;
    id encoder2 = [buffer renderCommandEncoderWithDescriptor:pass2];
    [encoder2 setRenderPipelineState:_quadState];
    [encoder2 setFragmentTexture:_albedoTexture atIndex:0];
    [encoder2 drawPrimitives:4 vertexStart:0 vertexCount:4];
    [encoder2 endEncoding];
    [buffer presentDrawable:drawable];
    [buffer commit];
  }
}

- (id<MTLRenderPipelineState>)createState:(NSString *)shader
{
  id library = [_device newLibraryWithSource:shader options:nil error:NULL];
  MTLRenderPipelineDescriptor *desc = [MTLRenderPipelineDescriptor new];
  desc.vertexFunction = [library newFunctionWithName:@"v_simple"];
  desc.fragmentFunction = [library newFunctionWithName:@"f_simple"];
  desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
  desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
  return [_device newRenderPipelineStateWithDescriptor:desc error:NULL];
}

- (void)createBuffers
{
  CGSize size = [_window.contentView frame].size;

  _depthTexture = [self createTexture:MTLPixelFormatDepth32Float_Stencil8 w:size.width h:size.height];
  _albedoTexture = [self createTexture:MTLPixelFormatRGBA8Unorm_sRGB w:size.width h:size.height];
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

- (void)mouseMoved:(NSEvent *)event
{
  if (![_window.contentView hitTest:[event locationInWindow]])
    [self toggleMouse:true];
  else if (_mouseMode == 1)
  {
    [self toggleMouse:false];
    _deltaX += [event deltaX];
    _deltaY += [event deltaY];
  }
}

- (void)mouseUp:(NSEvent *)event
{
  if (_mouseMode == 2)
    [self toggleMouse:true];
  else if ([event clickCount])
  {
    _clickX = [event locationInWindow].x;
    _clickY = [event locationInWindow].x;
  }
}

- (void)mouseDragged:(NSEvent *)event
{
  if (![_window.contentView hitTest:[event locationInWindow]] || _mouseMode != 2)
    return;

  [self toggleMouse:false];
  _deltaX += [event deltaX];
  _deltaY += [event deltaY];
}

- (void)windowWillClose:(NSWindow *)sender
{
  [NSApp terminate:nil];
}

- (void)toggleMouse:(bool)mode
{
  if (mode == _cursorVisible) return;

  mode ? [NSCursor unhide] : [NSCursor hide];
  CGAssociateMouseAndMouseCursorPosition(mode);
  _cursorVisible = mode;
}
@end

int main()
{
  @autoreleasepool
  {
    [NSApplication sharedApplication];

    // Prevent Sleeping
    IOPMAssertionID assertionID;
    IOPMAssertionCreateWithName(
        kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn,
        CFSTR("Application is an interactive game."), &assertionID);

    // Create the App Menu
    id appMenu = [[NSMenu new] autorelease];
    id servicesMenu = [[NSMenu alloc] autorelease];
    [[[appMenu addItemWithTitle:@"Services" action:NULL
                  keyEquivalent:@""] autorelease] setSubmenu:servicesMenu];
    [appMenu addItem:[[NSMenuItem separatorItem] autorelease]];
    [[appMenu addItemWithTitle:@"Hide"
                        action:@selector(hide:)
                 keyEquivalent:@"h"] autorelease];
    [[[appMenu addItemWithTitle:@"Hide Others"
                         action:@selector(hideOtherApplications:)
                  keyEquivalent:@"h"] autorelease]
        setKeyEquivalentModifierMask:NSEventModifierFlagOption |
                                     NSEventModifierFlagCommand];
    [[appMenu addItemWithTitle:@"Show All"
                        action:@selector(unhideAllApplications:)
                 keyEquivalent:@""] autorelease];
    [appMenu addItem:[[NSMenuItem separatorItem] autorelease]];
    [[appMenu addItemWithTitle:@"Quit"
                        action:@selector(terminate:)
                 keyEquivalent:@"q"] autorelease];
    [NSApp setServicesMenu:servicesMenu];

    // Create the Window Menu
    id windowMenu = [[[NSMenu alloc] initWithTitle:@"Window"] autorelease];
    [[windowMenu addItemWithTitle:@"Minimize"
                           action:@selector(performMiniaturize:)
                    keyEquivalent:@"m"] autorelease];
    [[windowMenu addItemWithTitle:@"Zoom"
                           action:@selector(performZoom:)
                    keyEquivalent:@"n"] autorelease];
    [[[windowMenu addItemWithTitle:@"Full Screen"
                            action:@selector(toggleFullScreen:)
                     keyEquivalent:@"f"] autorelease]
        setKeyEquivalentModifierMask:NSEventModifierFlagControl |
                                     NSEventModifierFlagCommand];
    [[windowMenu addItemWithTitle:@"Close Window"
                           action:@selector(performClose:)
                    keyEquivalent:@"w"] autorelease];
    [windowMenu addItem:[[NSMenuItem separatorItem] autorelease]];
    [[windowMenu addItemWithTitle:@"Bring All to Front"
                           action:@selector(arrangeInFront:)
                    keyEquivalent:@""] autorelease];
    [NSApp setWindowsMenu:windowMenu];

    // Create the Help Menu
    id helpMenu = [[[NSMenu alloc] initWithTitle:@"Help"] autorelease];
    [[helpMenu addItemWithTitle:@"Documentation"
                         action:@selector(docs:)
                  keyEquivalent:@""] autorelease];
    [NSApp setHelpMenu:helpMenu];

    // Create the Menu Bar
    id menubar = [[NSMenu new] autorelease];
    [[[menubar addItemWithTitle:@"" action:NULL
                  keyEquivalent:@""] autorelease] setSubmenu:appMenu];
    [[[menubar addItemWithTitle:@"Window" action:NULL
                  keyEquivalent:@""] autorelease] setSubmenu:windowMenu];
    [[[menubar addItemWithTitle:@"Help" action:NULL
                  keyEquivalent:@""] autorelease] setSubmenu:helpMenu];
    [NSApp setMainMenu:menubar];

    [NSApp setDelegate:[[App alloc] init]];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];

    return 0;
  }
}
