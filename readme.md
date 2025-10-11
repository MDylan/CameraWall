# CameraWall

A lightweight, desktop IP camera wall built with Qt 6.  
Show multiple RTSP/ONVIF cameras in a 2×2 / 3x2 / 3×3 grid, pop any tile to a full view, and get automatic reconnects and clear status feedback.

## Highlights

- **Grid view**: 2×2 / 3x2 / 3×3 pages; auto-rotate pages every 10 seconds when you have more cameras than fit.
- **Focus view**: Double-click a tile (or use the ⛶ button) to zoom to full view.  
  Press **Esc** to return to the grid. Use **← / →** to switch the focused camera (wrap-around).
- **Automatic reconnect**: If a stream stalls or errors, the app waits **5 seconds** and retries cleanly (stop → play).  
  It does **not** keep the last frame.
- **Clear status colors** on each tile:
  - **Green** – streaming OK
  - **Yellow** – connecting / retrying
  - **Red** – failed to connect
- **Configurable FPS limit**: Optional **15 FPS** throttling to reduce CPU usage.
- **Keep-alive option**: Keep background streams open while focusing, or pause them — your call.
- **RTSP & ONVIF**:
  - Add cameras by **RTSP URL** directly, or
  - Use **ONVIF** discovery (with cached stream URI support).
- **Noise-free logs**: Optional filtering of FFmpeg logs (see below).

## Screenshots

 - [Empty Main window](res/screenshots/01_main_window.jpg)
 - [Add new camera](res/screenshots/02_add_camera.jpg)
 - [Add new ONVIF camera](res/screenshots/03_add_camera.jpg)
 - [Get ONVIF stream profile](res/screenshots/04_get_camera_profile.jpg)
 - [Example camera added](res/screenshots/05_camera_added.jpg)

## Command Line options

- **Language selection**: --lang=hu|en
- **Screen selection**: --screen=1|2 etc.
- **Debugging**: --debug (It will create a .log file into the program folder)

## Controls & Shortcuts

- **Double-click** tile or click **⛶** → Enter/exit focus view
- **Esc** → Exit focus view back to grid
- **← / →** → While in focus view, switch to previous/next camera (wrap-around)
- **F11** → Toggle fullscreen window
- **Right-click** → Context menu (Add / Edit / Remove / Grid size / Reorder / Reload)
- **Menus** → Cameras / View / Help; language switching supported

## Status & Reconnect Logic

When a stream reports stall/error or demuxing failure:
- The tile turns **yellow** (connecting) and schedules a single retry after **5 seconds**.
- The player is restarted with **stop + play**.
- On success, status becomes **green**; on repeated failure, **red** is shown and retries continue on subsequent error callbacks.

## Building

**Requirements**
- Qt **6.9** (tested with 6.9.x)
  - Modules: `QtWidgets`, `QtMultimedia`, `QtNetwork`
- CMake **3.21+**
- A C++17 compiler (tested on Windows/MinGW; MSVC/Clang should work too)

**Build (CLI)**
```bash
# from repo root
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

**Build (Qt Creator)**
1. Open the CMake project.
2. Select a Qt 6.9 kit.
3. Build & Run.

## Adding Cameras

**RTSP (manual):**
- Right-click → **Add…**
- Paste your RTSP URL (with credentials if needed), e.g.  
  `rtsp://user:pass@192.168.1.50:554/Streaming/Channels/101`

**ONVIF:**
- Provide device/media endpoints and credentials.
- The selected profile token’s stream URI is cached and used for playback.

## Configuration & Persistence

- Settings (cameras and view options) are stored via **QSettings** (INI).
- The app persists:
  - Camera list (names, RTSP/ONVIF info, cached URIs)
  - View settings (grid size, FPS limit, auto-rotate, keep-alive)

> Tip: You can safely reorder cameras; the INI will be updated accordingly.

## Reducing FFmpeg Log Noise (optional)

If you want to suppress FFmpeg spam like “RTP: missed N packets”, add this at startup:

```cpp
QLoggingCategory::setFilterRules(QStringLiteral(
    "qt.multimedia.ffmpeg=false\n"
    "qt.multimedia.ffmpeg.demuxer=false\n"
    "qt.multimedia.ffmpeg.muxer=false\n"
    "qt.multimedia.ffmpeg.decoder=false\n"));
```

(Already wired in `main.cpp` for this project.)

## Known Good RTSP Patterns

- Hikvision: `rtsp://user:pass@CAM_IP:554/Streaming/Channels/101`
- Dahua: `rtsp://user:pass@CAM_IP:554/cam/realmonitor?channel=1&subtype=0`
- Generic: `rtsp://user:pass@CAM_IP:554/…`

Some cameras require enabling RTSP in their web UI and/or setting a **Main** vs **Sub** stream.

## Troubleshooting

- **Red status** or repeated **“Demuxing failed”**:
  - Try the sub-stream (lower bitrate).
  - Ensure camera time & network are stable; prefer **Unicast** RTSP.
  - If Wi-Fi, test with wired Ethernet.
- **High CPU**:
  - Enable **15 FPS** limit in **View → FPS limit 15**.
  - Use sub-stream and/or 2×2 grid.
- **Focus view stutters**:
  - Enable **Keep background streams** only if needed; otherwise pause them in the background.

## License

See [licence.txt](LICENSES/LICENSE.txt)
