//
//  ffdecoder.cpp
//  FFPlayer
//
//  The MIT License (MIT)
//
//  Copyright (c) 2016 Alexander Borovikov
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//

#include "ffdecoder.h"

#include <QDebug>

class FFDecoderPrivate {
public:
    FFDecoderPrivate() :
        videoCodecCtx(0),
        audioCodecCtx(0),
        swsContext(0),
        swrContext(0),
        swrBuffer(0),
        pFrame(0),
        pFrameRGB(0),
        buffer(0),
        videoStreamIndex(-1),
        audioStreamIndex(-1),
        frameDelayMsec(0),
        pixfmt(AV_PIX_FMT_RGB24)
    { }

public:
    AVCodecContext      *videoCodecCtx;
    AVCodecContext      *audioCodecCtx;
    struct SwsContext   *swsContext;
    SwrContext          *swrContext;
    void                *swrBuffer;
    AVFrame             *pFrame, *pFrameRGB;
    uint8_t             *buffer;

    int                 videoStreamIndex;
    int                 audioStreamIndex;
    double              frameDelayMsec;

    AVPixelFormat       pixfmt;
};

FFDecoder::FFDecoder(AVFormatContext *context, QObject *parent) :
    QObject(parent),
    d_ptr(new FFDecoderPrivate()) {

    // get a pointer to the codec context for the video or audio stream
    // find all streams that the library is able to decode
    d_ptr->videoStreamIndex = av_find_best_stream(context, AVMEDIA_TYPE_VIDEO, -1, -1, 0, 0);
    if (d_ptr->videoStreamIndex >= 0) {
        d_ptr->videoCodecCtx = context->streams[d_ptr->videoStreamIndex]->codec;
    }

    d_ptr->audioStreamIndex = av_find_best_stream(context, AVMEDIA_TYPE_AUDIO, -1, -1, 0, 0);
    if (d_ptr->audioStreamIndex >= 0) {
        d_ptr->audioCodecCtx = context->streams[d_ptr->audioStreamIndex]->codec;
    }

    // Load video codec
    AVCodec *videoDecoder = avcodec_find_decoder(d_ptr->videoCodecCtx->codec_id);
    if (!videoDecoder) {
        return;
    }

    if (avcodec_open2(d_ptr->videoCodecCtx, videoDecoder, 0) < 0 ) {
        return;
    }

    d_ptr->swsContext = sws_getCachedContext(0, d_ptr->videoCodecCtx->width, d_ptr->videoCodecCtx->height,
                                             d_ptr->videoCodecCtx->pix_fmt, d_ptr->videoCodecCtx->width,
                                             d_ptr->videoCodecCtx->height, d_ptr->pixfmt,
                                             SWS_FAST_BILINEAR, NULL, NULL, NULL);

    if (!d_ptr->swsContext) {
        return;
    }

    // Allocate video frame
    d_ptr->pFrame = av_frame_alloc();

    // Allocate an AVFrame structure
    d_ptr->pFrameRGB = av_frame_alloc();

    // Determine required buffer size and allocate buffer
    int numBytes = av_image_get_buffer_size(d_ptr->pixfmt, d_ptr->videoCodecCtx->width,
                                            d_ptr->videoCodecCtx->height, 1);
    d_ptr->buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
    // of AVPicture
    avpicture_fill((AVPicture *)d_ptr->pFrameRGB, d_ptr->buffer, d_ptr->pixfmt,
                   d_ptr->videoCodecCtx->width, d_ptr->videoCodecCtx->height);
}

FFDecoder::~FFDecoder() {
    // Free the RGB image
    if (d_ptr->buffer) {
        av_free(d_ptr->buffer);
    }

    if (d_ptr->pFrameRGB) {
        av_frame_free(&d_ptr->pFrameRGB);
    }

    // Free the YUV frame
    if (d_ptr->pFrame) {
        av_frame_free(&d_ptr->pFrame);
    }

    if (d_ptr->swsContext) {
        sws_freeContext(d_ptr->swsContext);
    }

    if (d_ptr->videoCodecCtx) {
        avcodec_close(d_ptr->videoCodecCtx);
    }
}

QList<FFFramePtr> FFDecoder::decodeFrames(AVPacket *packet) {
    QList<FFFramePtr> result;

    // decode frames from packet
    if (packet->stream_index == d_ptr->videoStreamIndex) {
        int gotframe = 0;
        int length = avcodec_decode_video2(d_ptr->videoCodecCtx, d_ptr->pFrame,
                                           &gotframe, packet);

        if (length <= 0) {
            return result;
        }

        if (gotframe) {
            sws_scale(d_ptr->swsContext, d_ptr->pFrame->data, d_ptr->pFrame->linesize, 0,
                      d_ptr->videoCodecCtx->height, d_ptr->pFrameRGB->data, d_ptr->pFrameRGB->linesize);

            QSharedPointer<FFVideoFrame> frame(new FFVideoFrame);
            frame->linesize = d_ptr->pFrameRGB->linesize[0];
            frame->width = d_ptr->videoCodecCtx->width;
            frame->height = d_ptr->videoCodecCtx->height;
            frame->position = av_frame_get_best_effort_timestamp(d_ptr->pFrame) * av_q2d(d_ptr->videoCodecCtx->time_base);

            // delay
            frame->frameDelayMsec = av_q2d(d_ptr->videoCodecCtx->time_base);
            frame->frameDelayMsec *= d_ptr->videoCodecCtx->ticks_per_frame;
            frame->frameDelayMsec += d_ptr->pFrame->repeat_pict * (frame->frameDelayMsec * 0.5);
            frame->frameDelayMsec *= 1000.0;

            // Convert the frame to QImage
            frame->image = QImage(d_ptr->videoCodecCtx->width, d_ptr->videoCodecCtx->height, QImage::Format_RGB888);

            for (int y = 0; y < frame->image.height(); y++) {
                memcpy(frame->image.scanLine(y), d_ptr->pFrameRGB->data[0] + y*d_ptr->pFrameRGB->linesize[0],
                        d_ptr->videoCodecCtx->width * 3);
            }

            result.append(frame);
        }
    }

    return result;
}

int FFDecoder::getFrameWidth() const {
    return d_ptr->videoCodecCtx ? d_ptr->videoCodecCtx->width : 0;
}

int FFDecoder::getFrameHeight() const {
    return d_ptr->videoCodecCtx ? d_ptr->videoCodecCtx->height : 0;
}
