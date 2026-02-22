// AudioEngine — pulls audio samples from the SNES APU via AVAudioEngine.
// Must include snes9x.h before Foundation to get uint8/bool8 typedefs.

#include "snes9x.h"
#include "apu/apu.h"

#import "AudioEngine.h"
#import <AVFoundation/AVFoundation.h>

@implementation AudioEngine {
    AVAudioEngine *_engine;
}

- (void)start {
    _engine = [[AVAudioEngine alloc] init];

    double sampleRate = Settings.SoundPlaybackRate ? Settings.SoundPlaybackRate : 48000;
    AVAudioFormat *format = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                                            sampleRate:sampleRate
                                                              channels:2
                                                           interleaved:YES];

    AVAudioSourceNode *srcNode = [[AVAudioSourceNode alloc]
        initWithFormat:format
        renderBlock:^OSStatus(BOOL *isSilence, const AudioTimeStamp *timestamp,
                              AVAudioFrameCount frameCount, AudioBufferList *outputData) {
            (void)isSilence;
            (void)timestamp;
            (void)frameCount;
            for (UInt32 i = 0; i < outputData->mNumberBuffers; i++) {
                AudioBuffer *buf = &outputData->mBuffers[i];
                int sampleCount = buf->mDataByteSize / 2;
                S9xMixSamples((uint8 *)buf->mData, sampleCount);
            }
            return noErr;
        }];

    [_engine attachNode:srcNode];
    [_engine connect:srcNode to:_engine.mainMixerNode format:format];

    NSError *error = nil;
    [_engine startAndReturnError:&error];
    if (error) {
        NSLog(@"AudioEngine start error: %@", error);
    }
}

- (void)stop {
    [_engine stop];
    _engine = nil;
}

@end
