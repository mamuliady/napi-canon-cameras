# Canon EOS RP Hardware Compatibility & Feature Test Report

This document records the automated hardware test results for the **Canon EOS RP** (Firmware 1.6.1) connected via USB to Apple Silicon macOS, using `@dimensional/napi-canon-cameras` with EDSDK 13.20.21.

---

## 1. Test Summary

| Metric | Result |
| :--- | :--- |
| **Camera Model** | Canon EOS RP |
| **Firmware Version** | 1.6.1 |
| **Lens Attached** | RF 50mm F1.8 STM |
| **Connection Interface** | USB (PTP / EDSDK) |
| **Total Features Tested** | 134 |
| **Passed Features** | 75 |
| **Failed Features** | 0 |
| **Unsupported / Hardware-Dependent** | 59 |

---

## 2. Working Features (Verified)

### A. Device Discovery & Lifecycle
- `cameraBrowser.getCameras()`: Successfully enumerates connected Canon EOS RP.
- `camera.description`: Returns `"Canon EOS RP"`.
- `camera.portName`: Returns port identifier `"0"`.
- `camera.connect()`: Establishes PTP session cleanly.
- `camera.disconnect()`: Closes session without leaving USB lockups.

### B. Live View (EVF)
- `camera.startLiveView()`: Activates EVF streaming mode.
- `camera.isLiveViewActive()`: Accurately reflects real-time EVF status (`true` when active, `false` when stopped).
- `camera.getLiveViewImage()`: Pulls live frames from the camera's electronic viewfinder.
- **Fast Binary Frame Retrieval:**
  - `image.getBlob()`: Returns raw binary JPEG bytes directly as a Node.js `Buffer` / `Uint8Array`.
  - `image.getJPEGBuffer()`: Alias for `getBlob()`, 100% byte-for-byte identical.
  - No Base64 CPU or memory conversion overhead.
- `image.getDataURL()`: Legacy Base64 data URL (`data:image/jpeg;base64,...`) maintained for backwards compatibility.
- `image.coordinateSystem`: Returns frame coordinate dimensions (e.g. 3888 x 2592).
- `image.zoom`: Returns current live view zoom factor.
- `image.zoomPosition`: Returns coordinate position of the live view zoom window.
- `camera.stopLiveView()`: Deactivates EVF streaming cleanly.

### C. Photo Capture & Host Transfer
- `camera.takePicture()`: Actuates camera shutter and captures full-resolution photo.
- `SaveTo.Host`: Allows capturing photos directly to host RAM and disk without needing an SD card inside the camera.
- Event Dispatch:
  - `Camera.EventName.DownloadRequest`
  - `Camera.EventName.FileCreate`
  - `Camera.EventName.PropertyChangeValue`
  - `Camera.EventName.PropertyChangeOptions`
  - `Camera.EventName.StateChange`
- `file.downloadToPath(outDir)`: Streams captured image file (JPEG or RAW) over USB directly to disk.
- Tested: Captured full 3888 x 2592 JPEG image saved and validated with Apple ImageIO `sips`.

### D. Camera Commands
- `camera.sendCommand(Camera.Command.ExtendShutDownTimer)`: Extends the camera body's auto-sleep timer.
- `camera.sendCommand(Camera.Command.PressShutterButton, Camera.PressShutterButton.Halfway)`: Activates autofocus and exposure metering (half-press).
- `camera.sendCommand(Camera.Command.PressShutterButton, Camera.PressShutterButton.OFF)`: Releases half-press.

### E. Readable Properties (48 Properties Verified)
- **Exposure & Optics**: `AEMode`, `AEModeSelect`, `AFMode`, `Av` (Aperture), `Tv` (Shutter Speed), `ISOSpeed`, `ExposureCompensation`, `MeteringMode`, `LensName` (`RF50mm F1.8 STM`), `LensStatus` (`Attached`).
- **Image Settings**: `ImageQuality` (e.g. `LargeJPEGFine`), `ColorSpace` (`sRGB`, `AdobeRGB`), `ColorTemperature` (e.g. 5200K), `WhiteBalance`, `WhiteBalanceShift`, `PictureStyle`, `PictureStyleDescription`, `Aspect`.
- **Hardware & Device State**: `ProductName`, `FirmwareVersion`, `BodyIDEx` (serial number), `BatteryLevel` (percentage), `BatteryQuality`, `TemperatureStatus` (0 = normal), `CurrentStorage` (`SD`), `AvailableShots`.
- **EVF Properties**: `Evf_Mode`, `Evf_OutputDevice`, `Evf_AFMode`, `Evf_ColorTemperature`, `Evf_WhiteBalance`, `Evf_DepthOfFieldPreview`.
- **Clock & Locale**: `DateTime`, `UTCTime`, `TimeZone`, `SummerTimeSetting`.

### F. Writable Properties (Verified)
- `ISOSpeed`: Can be set programmatically to any value in `allowedValues` or `Auto`.
- `WhiteBalance`: Can be set to Auto Ambience, Daylight, Shade, Cloudy, Tungsten, White Fluorescent, Flash, Custom, Color Temperature.
- `DriveMode`: Can be set to Single, High-Speed Continuous, Low-Speed Continuous, Self-Timer (10s, 2s, Continuous).
- `SaveTo`: Can be set to `Option.SaveTo.Host`, `Option.SaveTo.Camera`, or `Option.SaveTo.Both`.
- `Av` / `Tv`: Modifiable programmatically when body dial is in Manual (`M`) or relevant semi-auto mode.

---

## 3. Unsupported or Conditional Features

| Feature | Behavior on Canon RP | Reason / Recommendation |
| :--- | :--- | :--- |
| **`volume.getEntries()`** | Throws `DEVICE_DISK_ERROR` when empty | Normal behavior when **no SD card** is inserted into the camera card slot. Insert a formatted SD card to read filesystem entries. |
| **`image.histogram`** | Empty / not present | The camera does not generate live view histogram data unless the histogram overlay is enabled in the camera's body display settings. |
| **`MirrorLockUpState` / `MirrorUpSetting`** | Marked `unavailable` | The Canon EOS RP is a **mirrorless** camera and does not have a physical reflex mirror mechanism. |
| **`Flash*` / `FlashCompensation`** | Marked `unavailable` | The Canon EOS RP does not have a built-in pop-up flash. These properties only become available if an external Speedlite is attached to the hot-shoe. |
| **`GPS*` properties** | Marked `unavailable` | The Canon EOS RP has no built-in GPS receiver hardware. |
| **Body Dial Lock on `Av` / `Tv`** | Read-only in Auto mode | When the camera body mode dial is turned to full Auto / Scene intelligent mode, the camera firmware locks manual aperture and shutter overrides over USB. Set dial to `M`, `Av`, or `Tv`. |
| **macOS `ptpcamerad` Lock** | Camera list empty (`[]`) | macOS automatically launches `/usr/libexec/ptpcamerad` when a USB camera is plugged in. Run `pkill -9 ptpcamerad` before connecting. |

---

## 4. Running the Hardware Test Suite

To run the complete automated test suite against a connected camera:

```bash
# On macOS, release the USB interface from the OS daemon
pkill -9 ptpcamerad || true

# Execute the comprehensive test script
npx ts-node examples/test-canon-rp-full.ts
```

The script outputs color-coded test results and exports a full JSON log to `examples/canon-rp-test-results.json`.
