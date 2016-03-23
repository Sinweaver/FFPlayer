//
//  ffplayer_p.h
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

#ifndef FFPLAYERPRIVATE_H
#define FFPLAYERPRIVATE_H

#include <QObject>
#include <QMutex>
#include <QtConcurrent>

#include "ffplayer.h"
#include "ffheaders.h"
#include "ffdecoder.h"

class FFPlayerPrivate  : public QObject {
    Q_DECLARE_PUBLIC(FFPlayer)
public:
    explicit FFPlayerPrivate();
    virtual ~FFPlayerPrivate();

public:
    void open(const QUrl &url);
    void close();
    void play();
    void pause();
    void decodingFrames();

    void setState(const FFPlayer::FFPlayerState &state);
    FFPlayer::FFPlayerState getState() const;

    bool validVideo() const;
    bool validAudio() const;

    int getFrameWidth() const;
    int getFrameHeight() const;

    double getDuration() const;

    bool isNeedAbort() const;
    void setIsNeedAbort(bool isNeedAbort);

    FFPlayer                    *q_ptr;

private:
    AVFormatContext             *_formatContext;
    FFDecoderPtr                _decoder;

    FFPlayer::FFPlayerState     _state;
    bool                        _isNeedAbort;

    mutable QMutex              _contextLocker;
    mutable QMutex              _stateLocker;
    mutable QMutex              _abortLocker;
    QFutureWatcher<void>        _future_watcher;

    bool _openContent(const QUrl &url);
    void _closeContent();
};

#endif // FFPLAYERPRIVATE_H
