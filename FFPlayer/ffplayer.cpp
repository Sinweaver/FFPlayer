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

#include <QDebug>
#include <QtConcurrent>
#include <QSharedPointer>

#include "ffheaders.h"
#include "ffdecoder.h"

#define DEFAULT_INTERRUPT_TIMEOUT   300000       // 300 sec.
#define READ_INTERRUPT_TIMEOUT      10000        // 10 sec.

#define THREAD_SLEEP_TIMEOUT        1000         // 1 sec.

class FFPlayerPrivate  : public QObject {
    Q_DECLARE_PUBLIC(FFPlayer)
public:
    explicit FFPlayerPrivate();

    void open(const QUrl &url);
    AVFormatContext *openContext(const QUrl &url);
    void closeContext(AVFormatContext *formatContext);
    void decodeFrames(AVFormatContext *formatContext);

    bool isUserNeedAutoReconnect() const;
    void setIsUserNeedAutoReconnect(bool isUserNeedAutoReconnect);

    bool isReadyToReconnect() const;
    void setIsReadyToReconnect(bool isReadyToReconnect);

    void resetInterruptTimer(int timeoutInMsecs);

    bool isInterruptedByTimeout() const;
    void setIsInterruptedByTimeout(bool isInterruptedByTimeout);

    bool isInterruptedByUser() const;
    void setIsInterruptedByUser(bool isInterruptedByUser);

    qint64 interruptTimeMsec() const;
    void setInterruptTimeMsec(qint64 msecs);

    FFPlayer::State state() const;
    void setState(const FFPlayer::State &state);

public:
    FFPlayer             *q_ptr;
    QFutureWatcher<void> future_watcher;

private:
    FFPlayer::State      _state;

    bool                 _isReadyToReconnect;
    bool                 _isUserNeedAutoReconnect;

    bool                 _isInterruptedByTimeout;
    bool                 _isInterruptedByUser;
    qint64               _interruptTimeMsec; // in MSecs

    mutable QMutex       _stateMutex;
    mutable QMutex       _interruptMutex;
    mutable QMutex       _reconnectMutex;
};

static int decode_interrupt_cb(void *opaque) {
    FFPlayerPrivate *is = static_cast<FFPlayerPrivate *>(opaque);
    if (QDateTime::currentDateTime().toMSecsSinceEpoch() >= is->interruptTimeMsec()) {
        is->setIsInterruptedByTimeout(true);
    }

    return static_cast<int>(is->isInterruptedByTimeout() || is->isInterruptedByUser());
}

/*
 * FFPlayerPrivate
 */
FFPlayerPrivate::FFPlayerPrivate() :
    q_ptr(0),
    future_watcher(),
    _state(FFPlayer::StoppedState),
    _isReadyToReconnect(false),
    _isUserNeedAutoReconnect(false),
    _isInterruptedByTimeout(false),
    _isInterruptedByUser(false),
    _interruptTimeMsec(0),
    _stateMutex(QMutex::NonRecursive),
    _interruptMutex(QMutex::NonRecursive),
    _reconnectMutex(QMutex::NonRecursive) {

    class AVInitializer {
    public:
        AVInitializer() {
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

void FFPlayerPrivate::open(const QUrl &url) {
    Q_Q(FFPlayer);

    // Reset interrupt state.
    setIsInterruptedByUser(false);
    setIsInterruptedByTimeout(false);

    AVFormatContext *formatContext = openContext(url);
    if (!formatContext) {
        return;
    }

    setState(FFPlayer::PausedState);
    emit(q->contentDidOpened());

    // Start decoding frames.
    decodeFrames(formatContext);

    // Close and free context.
    closeContext(formatContext);

    setState(FFPlayer::StoppedState);
    emit(q->contentDidClosed());
}

AVFormatContext *FFPlayerPrivate::openContext(const QUrl &url) {
    // Allocate memory for AVFormatContext.
    AVFormatContext *formatContext = avformat_alloc_context();
    formatContext->interrupt_callback.callback = decode_interrupt_cb;
    formatContext->interrupt_callback.opaque = this;

    QString filePath = url.toString();

    // Set the options if its streaming video
    AVDictionary *rtmp_options = 0;

    QString urlScheme = url.scheme().toLower();
    if (urlScheme.compare("rtsp") || urlScheme.compare("rtmp")) {
        av_dict_set(&rtmp_options, "rtsp_transport", "tcp", 0);
    }

    // Open an input stream.
    // Set interrupt timeout.
    resetInterruptTimer(DEFAULT_INTERRUPT_TIMEOUT);
    if (avformat_open_input(&formatContext, filePath.toStdString().c_str(),
                            0, &rtmp_options) < 0) {
        avformat_free_context(formatContext);
        av_dict_free(&rtmp_options);
        return 0;
    }

    // Retrieve stream information
    // Set interrupt timeout.
    resetInterruptTimer(DEFAULT_INTERRUPT_TIMEOUT);
    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        av_dict_free(&rtmp_options);
        return 0;
    }

    av_dict_free(&rtmp_options);

    return formatContext;
}

void FFPlayerPrivate::closeContext(AVFormatContext *formatContext) {
    // Set interrupt timeout.
    resetInterruptTimer(DEFAULT_INTERRUPT_TIMEOUT);
    avformat_close_input(&formatContext);

    avformat_free_context(formatContext);
}

void FFPlayerPrivate::decodeFrames(AVFormatContext *formatContext) {
    Q_Q(FFPlayer);

    FFDecoder decoder(formatContext);

    while (!isInterruptedByTimeout() && !isInterruptedByUser()) {
        if (state() == FFPlayer::PlayingState) {
            // initialize packet, set data to NULL, let the demuxer fill it
            AVPacket packet;
            av_init_packet(&packet);
            packet.data = NULL;
            packet.size = 0;

            // read frames from the file
            // Set interrupt timeout.
            resetInterruptTimer(READ_INTERRUPT_TIMEOUT);
            int ret = av_read_frame(formatContext, &packet);

            // End of stream.
            if (ret < 0) {
                setIsReadyToReconnect(false);
                av_packet_unref(&packet);
                return;
            }

            // Connection lost.
            if (isInterruptedByTimeout()) {
                if (isUserNeedAutoReconnect()) {
                    setIsReadyToReconnect(true);
                }

                av_packet_unref(&packet);
                return;
            }

            QList<FFFramePtr> frames = decoder.decodeFrames(&packet);
            for (int i = 0; i < frames.count(); i++) {
                if (frames[i]->getFrameType() == FFFrame::FFFrameTypeVideo) {
                    QSharedPointer<FFVideoFrame> frame = qSharedPointerCast<FFVideoFrame>(frames[i]);

                    emit(q->updateVideoFrame(frame));
                }
            }

            av_packet_unref(&packet);
        }
        else {
            QThread::msleep(100);
        }
    }
}

void FFPlayerPrivate::resetInterruptTimer(int timeoutInMsecs) {
    QDateTime endDateTime = QDateTime::currentDateTime().addMSecs(timeoutInMsecs);
    setInterruptTimeMsec(endDateTime.toMSecsSinceEpoch());
}

FFPlayer::State FFPlayerPrivate::state() const {
    QMutexLocker stateLock(&_stateMutex);
    return _state;
}

void FFPlayerPrivate::setState(const FFPlayer::State &state) {
    QMutexLocker stateLock(&_stateMutex);
    _state = state;
}

bool FFPlayerPrivate::isUserNeedAutoReconnect() const {
    QMutexLocker stateLock(&_reconnectMutex);
    return _isUserNeedAutoReconnect;
}

void FFPlayerPrivate::setIsUserNeedAutoReconnect(bool isUserNeedAutoReconnect) {
    QMutexLocker stateLock(&_reconnectMutex);
    _isUserNeedAutoReconnect = isUserNeedAutoReconnect;
}

bool FFPlayerPrivate::isReadyToReconnect() const {
    return _isReadyToReconnect;
}

void FFPlayerPrivate::setIsReadyToReconnect(bool isReadyToReconnect) {
    _isReadyToReconnect = isReadyToReconnect;
}

bool FFPlayerPrivate::isInterruptedByUser() const {
    QMutexLocker abortLock(&_interruptMutex);
    return _isInterruptedByUser;
}

void FFPlayerPrivate::setIsInterruptedByUser(bool isInterruptedByUser) {
    QMutexLocker abortLock(&_interruptMutex);
    _isInterruptedByUser = isInterruptedByUser;
}

bool FFPlayerPrivate::isInterruptedByTimeout() const {
    return _isInterruptedByTimeout;
}

void FFPlayerPrivate::setIsInterruptedByTimeout(bool isInterruptedByTimeout) {
    _isInterruptedByTimeout = isInterruptedByTimeout;
}

qint64 FFPlayerPrivate::interruptTimeMsec() const {
    return _interruptTimeMsec;
}

void FFPlayerPrivate::setInterruptTimeMsec(qint64 msecs) {
    _interruptTimeMsec = msecs;
}

/*
 * FFPlayer
 */
FFPlayer::FFPlayer(QObject *parent) :
    QObject(parent),
    d_ptr(new FFPlayerPrivate) {

    Q_D(FFPlayer);
    d->q_ptr = this;

    qRegisterMetaType<FFVideoFramePtr>("FFVideoFramePtr");
}

FFPlayer::~FFPlayer() {
    close();
}

void FFPlayer::open(const QUrl &url) {
    Q_D(FFPlayer);

    if (d->future_watcher.isRunning()) {
#ifdef QT_DEBUG
        qWarning()<<"Is already opened!!!";
#endif
        return;
    }

    d->future_watcher.setFuture(QtConcurrent::run([this,url](){
        Q_D(FFPlayer);

        forever {
            d->open(url);

            if (d->isInterruptedByUser() || !d->isReadyToReconnect()) {
                break;
            }
            else {
                QThread::msleep(THREAD_SLEEP_TIMEOUT);
            }
        }
    }));
}

void FFPlayer::play() {
    Q_D(FFPlayer);

    d->setState(FFPlayer::PlayingState);
}

void FFPlayer::pause() {
    Q_D(FFPlayer);

    d->setState(FFPlayer::PausedState);
}

void FFPlayer::close() {
    Q_D(FFPlayer);

    if (d->future_watcher.isRunning()) {
        d->setIsInterruptedByUser(true);
        d->future_watcher.cancel();
        d->future_watcher.waitForFinished();
    }
}

FFPlayer::State FFPlayer::getState() const {
    Q_D(const FFPlayer);
    return d->state();
}

bool FFPlayer::isNeedAutoReconnect() const {
    Q_D(const FFPlayer);
    return d->isUserNeedAutoReconnect();
}

void FFPlayer::setIsNeedAutoReconnect(bool isNeedAutoReconnect) {
    Q_D(FFPlayer);
    d->setIsUserNeedAutoReconnect(isNeedAutoReconnect);
}
