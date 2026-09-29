# ONVIF Stream Discovery (Qt6)

A Qt6 application for discovering ONVIF camera stream URLs (RTSP and JPEG snapshots) using standard ONVIF SOAP API calls.

## Features

- **Stage 1**: GetSystemDateAndTime - device reachability check
- **Stage 2**: GetCapabilities - locate Media service XAddr
- **Stage 3**: GetProfiles - enumerate media profiles with video encoder configs (resolution, fps, codec, bitrate, GOV)
- **Stage 4**: GetStreamUri - retrieve RTSP stream URLs per profile (RTP-Unicast/RTSP)
- **Stage 5**: GetSnapshotUri - retrieve JPEG snapshot URLs per profile
- WS-Security UsernameToken (PasswordDigest) authentication
- Credentials embedded in URLs for convenience
- GUI and CLI modes

## Requirements

### Minimum Versions
- **Qt 6.2+** (tested with 6.11.2) - Core, Network, Widgets, Xml modules
- **C++17** compatible compiler (GCC 7+, Clang 5+, MSVC 19.1+)
- **CMake 3.16+** (if using CMake) or **qmake** (Qt 6)
- **OpenSSL 1.1.1+** or **3.x** (for HTTPS/TLS support)

### Required Qt 6 Modules
| Module | Purpose |
|--------|---------|
| `Qt6::Core` | Core functionality, XML handling, crypto |
| `Qt6::Network` | HTTP/SOAP communication, SSL/TLS |
| `Qt6::Widgets` | GUI components (tables, forms, dialogs) |
| `Qt6::Xml` | DOM/XML parsing for SOAP responses |

### Build Tools
- **make** / **ninja** (for building)
- **pkg-config** (for dependency detection on Linux)

### Runtime Dependencies
- Qt 6 runtime libraries (Core, Network, Widgets, Xml, Gui)
- OpenSSL libraries (libssl, libcrypto)
- Standard C++ library (libstdc++ / libc++)
- Network access to ONVIF cameras (port 80/443/8000/554)

### Optional
- **Qt 6 Linguist tools** (for translations, if adding i18n)

### Verified Platforms
| OS | Qt Version | Compiler | Status |
|----|------------|----------|--------|
| Ubuntu 22.04+ / Debian 12+ | 6.11.2 | GCC 11/12/13 | ✅ |
| Fedora 38+ | 6.11.2 | GCC 13 | ✅ |
| Arch Linux | 6.11.2 | GCC 13/14 | ✅ |
| Windows 10/11 | 6.11.2 | MSVC 2022 / MinGW | ✅ |
| macOS 12+ | 6.11.2 | Clang 14+ | ✅ |

## Build

```bash
cd <PROJECT_DIR>
<QT_PATH>/bin/qmake
make -j$(nproc)
```

*Replace `<PROJECT_DIR>` with the path to the project directory and `<QT_PATH>` with your Qt 6 installation path (e.g., `/home/user/Qt/6.11.2/gcc_64`).*

## Usage

### GUI Mode
```bash
./onvif_stream_discover
```
Opens a window with configuration fields and results tables.

### CLI Mode

**Named options:**
```bash
./onvif_stream_discover --cli --ip <CAMERA_IP> --user <USERNAME> --password <PASSWORD> --port <PORT> --timeout <SECONDS>
```

**Positional arguments:**
```bash
./onvif_stream_discover <CAMERA_IP> <USERNAME> <PASSWORD> --port=<PORT> --timeout=<SECONDS>
```

**Options:**
| Option | Short | Description | Default |
|--------|-------|-------------|---------|
| `--cli` | `-cli` | Run in CLI mode (no GUI) | - |
| `--ip` | `-i` | Camera IP address | - |
| `--user` | `-u` | Username | - |
| `--password` | `-p` | Password | - |
| `--port` | `-P` | Port | 80 |
| `--path` | | Service path | /onvif/device_service |
| `--timeout` | | Timeout in seconds | 5 |
| `--https` | | Use HTTPS | false |

## Output

### CLI Output Example
```
== ONVIF stream discovery for "<CAMERA_IP>" ==
Device service: "http://<CAMERA_IP>:<PORT>/onvif/device_service"

-- Checking device reachability --
  Device UTC time: "2026-9-29 9:28:34"

-- GetCapabilities (locating Media service) --
  Media service XAddr: "http://<CAMERA_IP>:<PORT>/onvif/media_service"

-- GetProfiles --
  Profile token: "000"   (name: "Profile000_MainStream" )
    Video: "2560x1920, 10 fps, H264 (Main), 4096 kbps, GOV 10, quality 0"
  Profile token: "001"   (name: "Profile001_SubStream" )
    Video: "640x480, 7 fps, H264 (Main), 160 kbps, GOV 10, quality 2"

-- GetStreamUri / GetSnapshotUri per profile --
  [ "Profile000_MainStream" ] token= "000"
  [ "Profile001_SubStream" ] token= "001"
    Snapshot: "http://<USER>:***@<CAMERA_IP>:80/cgi-bin/api.cgi?cmd=onvifSnapPic&channel=0"
    RTSP: "rtsp://<USER>:***@<CAMERA_IP>:554/h264Preview_01_main"
    (raw, no creds): "rtsp://<CAMERA_IP>:554/h264Preview_01_main"
    RTSP: "rtsp://<USER>:***@<CAMERA_IP>:554/h264Preview_01_sub"
    (raw, no creds): "rtsp://<CAMERA_IP>:554/h264Preview_01_sub"
    Snapshot: "http://<USER>:***@<CAMERA_IP>:80/cgi-bin/api.cgi?cmd=onvifSnapPic&channel=0"

== Summary ==
-  "Profile000_MainStream"
    RTSP:      "rtsp://<USER>:***@<CAMERA_IP>:554/h264Preview_01_main"
    Snapshot:  "http://<USER>:***@<CAMERA_IP>:80/cgi-bin/api.cgi?cmd=onvifSnapPic&channel=0"
     "2560x1920, 10 fps, H264 (Main), 4096 kbps, GOV 10, quality 0"
-  "Profile001_SubStream"
    RTSP:      "rtsp://<USER>:***@<CAMERA_IP>:554/h264Preview_01_sub"
    Snapshot:  "http://<USER>:***@<CAMERA_IP>:80/cgi-bin/api.cgi?cmd=onvifSnapPic&channel=0"
     "640x480, 7 fps, H264 (Main), 160 kbps, GOV 10, quality 2"

Use these URLs directly in VLC, ffmpeg, or your NVR/recording software.
```

### GUI Features
- **Camera Configuration** panel: IP, port, path, credentials, HTTPS, timeout
- **Media Profiles** table: profile token, name, resolution, FPS, codec, bitrate, GOV
- **Stream URLs** table: profile, type (Video Stream/Snapshot), URL (with masked credentials)
- **Log Output**: timestamped operation log

## URL Types

| Type | Description |
|------|-------------|
| Video Stream (RTSP) | RTSP URL with embedded credentials |
| Video Stream (raw) | Raw RTSP URL without credentials |
| Snapshot (JPEG) | HTTP URL for JPEG snapshots with embedded credentials |

## Notes

- The application uses WS-Security PasswordDigest authentication (standard ONVIF)
- SSL certificate verification is disabled (many cameras use self-signed certificates)
- Tested with multiple cameras - works correctly with valid credentials
- Default credentials `admin/admin` may not work - check camera documentation

## Project Structure

```
onvif_stream_discover/
├── main.cpp           # Entry point, CLI/GUI mode handling
├── MainWindow.h/cpp   # Main window with UI logic
├── MainWindow.ui      # Qt Designer UI file
├── OnvifClient.h/cpp  # ONVIF SOAP client with async requests
├── OnvifModels.h/cpp  # SOAP envelope building & XML parsing
├── WsSecurity.h/cpp   # WS-Security UsernameToken (PasswordDigest)
├── StreamInfo.h       # Data structures
├── onvif_stream_discover.pro # qmake project file
└── README.md          # This file
```