//
//  main.cpp
//  WebrtcApmDemo
//
//  Created by 石硕 on 16/9/22.
//  Copyright © 2016年 shishuo. All rights reserved.
//

#include <iostream>
#include "webrtc/modules/audio_processing/aec/echo_cancellation.h"
#include "webrtc/modules/audio_processing/aecm/echo_control_mobile.h"
#include "webrtc/modules/audio_processing/agc/legacy/gain_control.h"
#include "webrtc/modules/audio_processing/ns/noise_suppression.h"
#include "webrtc/modules/audio_processing/ns/noise_suppression_x.h"
#include "webrtc/modules/utility/include/audio_frame_operations.h"

#define SAMPLE_BITS 16  // 采样位宽/位深，只支持816

void testAECProcess(const char* mic_pcm,
                    const char* ref_pcm,
                    const char* out_pcm,
                    int sampleFrequence,
                    int channels,
                    int delay_ms);

int main(int argc, const char * argv[]) {
    
    const char* mic_pcm = "mic.pcm";  // 麦克风录制的混音文件
    const char* ref_pcm = "ref.pcm";  // 参考声音(回声)文件
    const char* out_pcm = "out.pcm";  // 原始声音文件
    testAECProcess(mic_pcm, ref_pcm, out_pcm,
                   8000, 1, 40);
    
    // 此处由于缺少合适的测试文件，只尝试了采样率8000Hz，其他采样率和双声道并未测试；
    
    return 0;
}

// 回声消除功能
// @mic_pcm 麦克风录制的混音文件
// @ref_pcm 参考声音(回声)文件，数据长度应该与麦克风录制的混音文件相等
// @out_pcm 原始声音文件
// @sampleFrequence 采样率，只支持8000, 16000, 32000, 48000
// @channels 声道
// @delay_ms 麦克风录制的回声声波比参考声音声波延迟的时间，单位毫秒，
//    PC电脑在10ms左右，苹果手机在20ms左右，安卓手机在100ms左右。
void testAECProcess(const char* mic_pcm,
                    const char* ref_pcm,
                    const char* out_pcm,
                    int sampleFrequence,
                    int channels,
                    int delay_ms) {
    
    int i = 0, sampleChunkBytes;
    const int sampleNums = 160; // 一次处理的采样数量，只支持80, 160
    FILE* ref_fd = 0, *mic_fd = 0, *out_fd = 0;
    short *mic_buf = 0, *ref_buf = 0, *out_buf = 0;
    float *aec_mic_buf = 0, *aec_ref_buf = 0, *aec_out_buf = 0;
    
    if (channels == 1 || channels == 2) {
    } else {
        printf("Unsupport channels. (only 1 or 2)\n");
        return;
    }
    
    // SAMPLE_BITS 16
    sampleChunkBytes = 2 * sampleNums * channels;
        mic_buf = (short*)malloc(sampleChunkBytes + 1);
        ref_buf = (short*)malloc(sampleChunkBytes + 1);
        out_buf = (short*)malloc(sampleChunkBytes + 1);
        aec_mic_buf = (float*)malloc(sampleChunkBytes * sizeof(float) + 1);
        aec_ref_buf = (float*)malloc(sampleChunkBytes * sizeof(float) + 1);
        aec_out_buf = (float*)malloc(sampleChunkBytes * sizeof(float) + 1);
    
    if (sampleFrequence == 8000
        || sampleFrequence == 16000
        || sampleFrequence == 32000
        || sampleFrequence == 48000) {
    } else {
        printf("Unsupport sample frequence. (only 8000 or 16000 or 32000 or 48000)\n");
        return;
    }
    
    ref_fd = fopen (ref_pcm, "rb");   // 参考声音(回声)
    mic_fd = fopen (mic_pcm, "rb");   // 麦克风混合声音
    out_fd = fopen (out_pcm, "wb");   // 原始声音
    
    if(ref_fd == NULL || mic_fd == NULL || out_pcm == NULL)
    {
        printf("File open error.\n");
        return;
    }
    
    webrtc::Aec* aec = NULL;
    webrtc::AecConfig config;
    
    aec = (webrtc::Aec*)webrtc::WebRtcAec_Create();
    webrtc::WebRtcAec_Init(aec, sampleFrequence, sampleFrequence);
    webrtc::WebRtcAec_set_config(aec, config);
    
    while (fread(mic_buf, 1, sampleChunkBytes, mic_fd))
    {
        fread(ref_buf, 1, sampleChunkBytes, ref_fd);
        
        if (2 == channels)
        {
            webrtc::AudioFrameOperations::StereoToMono(mic_buf, sampleNums, mic_buf);
            webrtc::AudioFrameOperations::StereoToMono(ref_buf, sampleNums, ref_buf);
        }
        
            for(i = 0; i < sampleNums; i++)
            {
                aec_mic_buf[i] = (float)mic_buf[i];
                aec_ref_buf[i] = (float)ref_buf[i];
            }
        // fill ref buffer to L band
        webrtc::WebRtcAec_BufferFarend(aec,
                                       (const float*)aec_ref_buf,
                                       sampleNums);
        // aec process
        float const * aec_mic_buf_ptr[] = { aec_mic_buf, 0 };
        float const * aec_out_buf_prt[] = { aec_out_buf, 0 };
        webrtc::WebRtcAec_Process(aec,
                                  (const float *const *)aec_mic_buf_ptr,
                                  1,    // 波形，理论上有多少个声道/单轨，就有多少个波形
                                  (float *const *)aec_out_buf_prt,
                                  sampleNums,
                                  delay_ms,
                                  0);
        
        // copy to out buffer
        for(i = 0; i < sampleNums; i++)
            out_buf[i] = (short)aec_out_buf[i];
        
        // 双声道处理，这里暂时有问题，还没处理
        if (2 == channels)
        {
            short tmp[sampleNums + 1] = {0};
            for(i = 0; i < sampleNums; i++)
                tmp[i] = out_buf[i];
            webrtc::AudioFrameOperations::MonoToStereo(tmp, sampleNums, out_buf);
        }
        
        // write out file
        fwrite(out_buf, 1, sampleChunkBytes, out_fd);
    }
    
    webrtc::WebRtcAec_Free(aec);
    fclose(mic_fd);
    fclose(ref_fd);
    fclose(out_fd);
    if (mic_buf) {free(mic_buf); mic_buf = NULL;}
    if (ref_buf) {free(ref_buf); ref_buf = NULL;}
    if (out_buf) {free(out_buf); out_buf = NULL;}
    if (aec_mic_buf) {free(aec_mic_buf); aec_mic_buf = NULL;}
    if (aec_ref_buf) {free(aec_ref_buf); aec_ref_buf = NULL;}
    if (aec_out_buf) {free(aec_out_buf); aec_out_buf = NULL;}
}
