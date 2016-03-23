//
//  ffdecoder.h
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

#ifndef FFVIDEODECODER_H
#define FFVIDEODECODER_H

#include <QObject>
#include <QScopedPointer>
#include <QSharedPointer>

#include "ffheaders.h"
#include "ffvideoframe.h"
#include "ffaudioframe.h"

class FFDecoderPrivate;
class FFDecoder : public QObject
{
    Q_OBJECT
public:
    explicit FFDecoder(AVFormatContext *context, QObject *parent = 0);
    virtual ~FFDecoder();

    QList<FFFramePtr> decodeFrames(AVPacket *packet);

    int getFrameWidth() const;
    int getFrameHeight() const;

signals:

public slots:

private:
    QScopedPointer<FFDecoderPrivate> d_ptr;
};

typedef QSharedPointer<FFDecoder> FFDecoderPtr;

#endif // FFVIDEODECODER_H
