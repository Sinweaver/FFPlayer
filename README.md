# FFPlayer

FFPlayer is a multimedia playback library based on Qt and FFmpeg.

## Usage

Drop files from FFPlayer folder in your project and in the source files where you need to use the library, import the header file:

```cpp
#include "ffplayer.h"
```

### Initialization

```cpp
player = new FFPlayer();
connect(player, &FFPlayer::contentDidOpened, this, &MainWindow::contentDidOpened);
connect(player, &FFPlayer::contentDidClosed, this, &MainWindow::contentDidClosed);

connect(player, &FFPlayer::updateVideoFrame, this, &MainWindow::updateVideoFrame);
```

### Open (Non-blocking)

```cpp
player->open(QUrl("rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov"));
```

### Play

```cpp
void MainWindow::contentDidOpened() {
    player->play();
}
```

## License

FFPlayer is available under the MIT license. See the LICENSE file for more info.