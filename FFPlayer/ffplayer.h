//
//  ffplayer.h
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

#ifndef FFDEMUXER_H
#define FFDEMUXER_H

#include <QUrl>
#include <QScopedPointer>
#include <QSharedPointer>

#include "ffvideoframe.h"
#include "ffaudioframe.h"

class FFPlayerPrivate;
class FFPlayer : public QObject {
    Q_OBJECT
public:
    /** Playback state */
    typedef enum {
        FFPlayerStateClosed = 0,
        FFPlayerStatePlaying,
        FFPlayerStatePaused,
    } FFPlayerState;

    explicit FFPlayer(QObject *parent = 0);
    virtual ~FFPlayer();

    void open(const QUrl &url);
    void close();
    void play();
    void pause();

    double getFrameRate() const;
    double getDuration() const;

    int getFrameWidth() const;
    int getFrameHeight() const;

    FFPlayerState getState() const;

signals:
    void updateVideoFrame(FFVideoFramePtr frame);
    void stateChanged(FFPlayerState status);

    void contentDidOpened();
    void contentDidClosed();

public slots:

protected:
    QScopedPointer<FFPlayerPrivate> d_ptr;

private:
    Q_DECLARE_PRIVATE(FFPlayer)
    Q_DISABLE_COPY(FFPlayer)
};

typedef QSharedPointer<FFPlayer> FFPlayerPtr;
Q_DECLARE_METATYPE(FFVideoFramePtr)

#endif // FFDEMUXER_H
