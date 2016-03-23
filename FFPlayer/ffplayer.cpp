//
//  ffplayer.cpp
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

#include "ffplayer.h"
#include "ffplayer_p.h"

#include <QDebug>
#include <QMutexLocker>
#include <limits>

/*
 * FFPlayerPrivate
 */
FFPlayerPrivate::FFPlayerPrivate() :
    q_ptr(0),
    _formatContext(0),
    _decoder(),
    _state(FFPlayer::FFPlayerStateClosed),
    _isNeedAbort(false),
    _contextLocker(QMutex::NonRecursive),
    _stateLocker(QMutex::NonRecursive),
    _abortLocker(QMutex::NonRecursive) {

    class AVInitializer {
    public:
        AVInitializer() {
            qRegisterMetaType<FFVideoFramePtr>("FFVideoFramePtr");

            // Initialize library ffmpeg using av_register_all ().
            // During initialization, recorded all available in the library
            // File formats and codecs.
            avcodec_register_all();
            av_register_all();

            // initialization of network components
            avformat_network_init();
        }
        ~AVInitializer() {
            //
            avformat_network_deinit();
        }
    };
    static AVInitializer sAVInit;
    Q_UNUSED(sAVInit);

#ifdef QT_DEBUG
    av_log_set_level(AV_LOG_VERBOSE);
#else
    av_log_set_level(AV_LOG_QUIET);
#endif
}

FFPlayerPrivate::~FFPlayerPrivate() {
    close();
}

bool FFPlayerPrivate::_openContent(const QUrl &url) {
    Q_Q(FFPlayer);

    setIsNeedAbort(false);

    // Set the options if its streaming video
    AVDictionary *rtmp_options = 0;

    QString urlScheme = url.scheme().toLower();
    if (urlScheme.compare("rtsp") || urlScheme.compare("rtmp")) {
        av_dict_set(&rtmp_options, "rtsp_transport", "tcp", 0);
    }

    // выделяем память под AVFormatContext.
    AVFormatContext *formatContext = avformat_alloc_context();

    // открываем Movie.
    QString filePath = url.toString();
    if (avformat_open_input(&formatContext, filePath.toStdString().c_str(),
                            0, &rtmp_options) < 0) {
        av_dict_free(&rtmp_options);
        return false;
    }

    av_dict_free(&rtmp_options);

    // retrieve stream information
    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        avformat_close_input(&formatContext);
        return false;
    }

    QMutexLocker l(&_contextLocker);
    _formatContext = formatContext;
    _decoder = FFDecoderPtr(new FFDecoder(_formatContext));

    emit(q->contentDidOpened());

    return true;
}

void FFPlayerPrivate::_closeContent() {
    Q_Q(FFPlayer);

    QMutexLocker l(&_contextLocker);
    _decoder.clear();

    if (_formatContext) {
        avformat_close_input(&_formatContext);
        _formatContext = 0;
    }
    l.unlock();

    setState(FFPlayer::FFPlayerStateClosed);
    emit(q->contentDidClosed());
}

void FFPlayerPrivate::open(const QUrl &url) {
    if (_future_watcher.isRunning()) {
        return;
    }

    _future_watcher.setFuture(QtConcurrent::run([this,url](){
        Q_Q(FFPlayer);

        if (!_openContent(url)) {
            return;
        }

        setState(FFPlayer::FFPlayerStatePaused);

        // initialize packet, set data to NULL, let the demuxer fill it
        AVPacket packet;
        av_init_packet(&packet);
        packet.data = NULL;
        packet.size = 0;

        while (!isNeedAbort()) {
            if (getState() == FFPlayer::FFPlayerStatePlaying) {
                QMutexLocker l(&_contextLocker);
                // read frames from the file
                bool isEOF = av_read_frame(_formatContext, &packet) < 0;
                if (isEOF) {
                    break;
                }

                QList<QSharedPointer<FFFrame>> frames = _decoder->decodeFrames(&packet);
                int timeout = 0;
                for (int i = 0; i < frames.count(); i++) {
                    if (frames[i]->getFrameType() == FFFrame::FFFrameTypeVideo) {
                        QSharedPointer<FFVideoFrame> frame = qSharedPointerCast<FFVideoFrame>(frames[i]);
                        float interval = frame->frameDelayMsec;
                        timeout = (int)qMax(interval, 0.0f);
                        emit(q->updateVideoFrame(frame));
                    }
                }
            }
        }

        av_packet_unref(&packet);

        _closeContent();
    }));
}

void FFPlayerPrivate::close() {
    setIsNeedAbort(true);

    if (_future_watcher.isRunning()) {
        _future_watcher.cancel();
        _future_watcher.waitForFinished();
    }
}

void FFPlayerPrivate::play() {
    if (getState() != FFPlayer::FFPlayerStatePlaying) {
        setState(FFPlayer::FFPlayerStatePlaying);
    }
}

void FFPlayerPrivate::pause() {
    if (getState() == FFPlayer::FFPlayerStatePlaying) {
        setState(FFPlayer::FFPlayerStatePaused);
    }
}

bool FFPlayerPrivate::isNeedAbort() const {
    QMutexLocker l(&_abortLocker);
    return _isNeedAbort;
}

void FFPlayerPrivate::setIsNeedAbort(bool isNeedAbort) {
    QMutexLocker l(&_abortLocker);
    _isNeedAbort = isNeedAbort;
}

void FFPlayerPrivate::setState(const FFPlayer::FFPlayerState &state) {
    Q_Q(FFPlayer);

    QMutexLocker l(&_stateLocker);
    _state = state;
    emit(q->stateChanged(_state));
}

FFPlayer::FFPlayerState FFPlayerPrivate::getState() const {
    QMutexLocker l(&_stateLocker);
    return _state;
}

bool FFPlayerPrivate::validVideo() const {
    QMutexLocker l(&_contextLocker);
    return !_decoder.isNull();
}

bool FFPlayerPrivate::validAudio() const {
    QMutexLocker l(&_contextLocker);
    return !_decoder.isNull();
}

int FFPlayerPrivate::getFrameWidth() const {
    QMutexLocker l(&_contextLocker);
    return !_decoder.isNull() ? _decoder->getFrameWidth() : 0;
}

int FFPlayerPrivate::getFrameHeight() const {
    QMutexLocker l(&_contextLocker);
    return !_decoder.isNull() ? _decoder->getFrameHeight() : 0;
}

double FFPlayerPrivate::getDuration() const {
    QMutexLocker l(&_contextLocker);
    if (!_formatContext) {
        return 0;
    }

    if (_formatContext->duration == AV_NOPTS_VALUE) {
        return std::numeric_limits<float>::max();
    }

    return (double)_formatContext->duration / (double)AV_TIME_BASE;
}

/*
 * FFPlayer
 */
FFPlayer::FFPlayer(QObject *parent) :
    QObject(parent),
    d_ptr(new FFPlayerPrivate) {

    Q_D(FFPlayer);
    d->q_ptr = this;
}

FFPlayer::~FFPlayer() {

}

void FFPlayer::open(const QUrl &url) {
    Q_D(FFPlayer);
    d->open(url);
}

void FFPlayer::play() {
    Q_D(FFPlayer);
    d->play();
}

void FFPlayer::pause() {
    Q_D(FFPlayer);
    d->pause();
}

void FFPlayer::close() {
    Q_D(FFPlayer);
    d->close();
}

double FFPlayer::getFrameRate() const {
    return 0.0;
}

double FFPlayer::getDuration() const {
    Q_D(const FFPlayer);
    return d->getDuration();
}

int FFPlayer::getFrameWidth() const {
    Q_D(const FFPlayer);
    return !d->getFrameWidth();
}

int FFPlayer::getFrameHeight() const {
    Q_D(const FFPlayer);
    return !d->getFrameHeight();
}

FFPlayer::FFPlayerState FFPlayer::getState() const {
    Q_D(const FFPlayer);
    return d->getState();
}
