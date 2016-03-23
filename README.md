# FFPlayer

FFPlayer is a multimedia playback library based on Qt and FFmpeg.

## Usage

1. Build FFMpeg with using [scripts directory](https://github.com/Sinweaver/FFPlayer/tree/master/scripts) ([Zeranoe](https://ffmpeg.zeranoe.com/builds/) for Windows).
2. [Download repository](https://github.com/Sinweaver/FFPlayer/archive/master.zip)
3. Drop files from [FFPlayer directory](https://github.com/Sinweaver/FFPlayer/tree/master/FFPlayer) in your project.
4. In the source files where you need to use the library, import the header file:

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