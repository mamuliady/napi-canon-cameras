# @dimensional/napi-canon-cameras

Node.js N-API native addon module for Canon EOS digital cameras, wrapping the official Canon EDSDK (v13.20.21 / v13.18.00).

Supports **macOS** (Apple Silicon `arm64` and Intel `x64`) and **Windows** (x64 and ia32).

* [Features](#features)
* [Usage Examples](#usage)
  * [Live View with getBlob() / getJPEGBuffer()](#live-view-streaming)
  * [Take Picture & Download to Host](#take-picture)
* [Comparison with Previous Tarball (napi-canon-cameras-41.tgz)](#comparison-with-previous-tarball)
* [macOS Specifics & Troubleshooting](#macos-setup--troubleshooting)
* [Hardware Compatibility Report (Canon EOS RP)](#hardware-compatibility)
* [Build & Package](#build--package)
  * [NPM Tasks](#npm-tasks)
* [API Documentation](API.md)

---

## Features

- [x] **Platform Support**:
  - macOS (Apple Silicon `arm64` + Intel `x64`) with signed `EDSDK.framework`
  - Windows (x64 + ia32) with dynamic `EDSDK.dll`
- [x] **Camera Discovery & Browser**:
  - Enumerate connected Canon cameras (`cameraBrowser.getCameras()`)
  - Hotplug device detection events (`CameraAdd`, `CameraRemove`)
- [x] **Live View (EVF)**:
  - **High-Performance Binary Buffer**: `image.getBlob()` and `image.getJPEGBuffer()` return raw JPEG binary buffers directly without base64 encoding overhead
  - Legacy Base64: `image.getDataURL()`
  - Real-time status: `camera.isLiveViewActive()`
  - Frame metadata: zoom factor, focus/zoom coordinates, coordinate system dimensions
- [x] **Capture & File Transfer**:
  - `camera.takePicture()`
  - Save directly to host RAM/disk (`Option.SaveTo.Host`) without needing an SD card
  - Save to camera SD card (`Option.SaveTo.Camera`) or both (`Option.SaveTo.Both`)
  - Direct file download: `file.downloadToPath(dir)`
- [x] **Camera Properties**:
  - Read & Write ISO sensitivity (`ISOSpeed`)
  - Read & Write White Balance (`WhiteBalance`)
  - Read & Write Drive Mode (`DriveMode`)
  - Read & Write Aperture (`Av`) & Shutter Speed (`Tv`)
  - Read Battery Level, Firmware Version, Lens Name, Serial Number, Temperature Status
- [x] **Storage & Card Access**:
  - Enumerate storage volumes (`camera.getVolumes()`)
  - Query volume metadata (label, capacity, read/write status)
  - Recursive directory tree and file browsing (when SD card is inserted)

---

## Usage

### Live View Streaming

```typescript
import { cameraBrowser, CameraProperty, Option } from '@dimensional/napi-canon-cameras';

// Obtain first available camera
const camera = cameraBrowser.getCamera();
if (!camera) {
    throw new Error('No Canon camera found.');
}

camera.connect();

// Verify Live View capability
if (camera.getProperty(CameraProperty.ID.Evf_Mode).available) {
    camera.startLiveView();

    // Pull live view frames in a loop or timer
    const interval = setInterval(() => {
        const image = camera.getLiveViewImage();
        if (image) {
            // Get raw JPEG bytes as Buffer / Uint8Array (no Base64 overhead!)
            const jpegBuffer = image.getBlob(); // or image.getJPEGBuffer()
            console.log(`Frame received: ${jpegBuffer.length} bytes (dimensions: ${image.coordinateSystem.width}x${image.coordinateSystem.height})`);

            // Example: write frame directly to disk or send over WebSocket/HTTP
            // fs.writeFileSync('live-frame.jpg', jpegBuffer);
        }
    }, 60);

    // Stop after some time
    setTimeout(() => {
        clearInterval(interval);
        camera.stopLiveView();
        camera.disconnect();
    }, 5000);
}
```

### Take Picture

```typescript
import {
    cameraBrowser,
    Camera,
    CameraProperty,
    Option,
    ImageQuality,
    watchCameras
} from '@dimensional/napi-canon-cameras';

const camera = cameraBrowser.getCamera();
if (!camera) throw new Error('No camera found.');

// Listen for download requests
camera.setEventHandler((eventName, event) => {
    if (eventName === Camera.EventName.DownloadRequest || eventName === Camera.EventName.FileCreate) {
        const file = (event as any).file;
        console.log(`Downloading ${file.name}...`);
        file.downloadToPath(__dirname + '/downloads');
        console.log('Download complete!');
        process.exit(0);
    }
});

camera.connect();

// Configure capture target to Host (works without SD card)
camera.setProperties({
    [CameraProperty.ID.SaveTo]: Option.SaveTo.Host,
    [CameraProperty.ID.ImageQuality]: ImageQuality.ID.LargeJPEGFine
});

// Watch events & fire shutter
const unwatch = watchCameras(50);
camera.takePicture();
```

---

## macOS Setup & Troubleshooting

### Exclusive USB Claim by `ptpcamerad`

When a Canon camera is connected via USB to a Mac, macOS's system daemon (`/usr/libexec/ptpcamerad`) automatically claims exclusive control of the USB PTP interface. This causes Canon's EDSDK to return `DEVICE_NOT_FOUND` or an empty list `[]`.

**Solution**:
Terminate `ptpcamerad` before connecting:
```bash
pkill -9 ptpcamerad || true
```
Alternatively, in Node.js scripts before opening camera sessions:
```javascript
const { execSync } = require('child_process');
try { execSync('pkill -9 ptpcamerad 2>/dev/null'); } catch (e) {}
```

---

## Hardware Compatibility

Full automated hardware validation was executed against a physical **Canon EOS RP** (Firmware 1.6.1 with RF 50mm F1.8 STM lens) over USB on Apple Silicon macOS.

- **Total features tested**: 134
- **Passed**: 75
- **Failed**: 0
- **Hardware-dependent / Unsupported**: 59 (e.g. mirror lockup on mirrorless bodies, external flash properties without attached speedlite, and filesystem browsing when no SD card is inserted).

Detailed breakdown of all tested properties and commands is available in [CANON_EOS_RP_COMPATIBILITY.md](docs/CANON_EOS_RP_COMPATIBILITY.md).

---

## Build & Package

### Prerequisites
- Node.js >= 18
- Python 3 and C++ build tools (`make` / Xcode command line tools on macOS, Visual Studio on Windows)
- Canon EDSDK placed in `third_party/EDSDK`:
  - macOS: `third_party/EDSDK/EDSDK.framework`
  - Windows: `third_party/EDSDK/Dll/` and `third_party/EDSDK/Library/`

### NPM Tasks

* `npm run prebuild:darwin` - Compile universal macOS prebuilds (`arm64` and `x64`) and build stubs
* `npm run prebuild:win32` - Compile Windows prebuilds (`ia32` and `x64`) and build stubs
* `npm run build:stubs` - Regenerate TypeScript definitions and JavaScript stubs
* `npm test` - Run full Jest test suite
* `npm run lint` - Run ESLint code checks
* `npm run package` - Generate distributable `.tgz` package in parent directory

---

## License

GPL-3.0-or-later
