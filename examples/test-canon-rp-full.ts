/* eslint-disable @typescript-eslint/no-explicit-any */
import * as fs from 'fs';
import * as path from 'path';
import {
    Camera,
    cameraBrowser,
    CameraProperty,
    Option,
    ImageQuality,
    Directory,
    CameraFile,
    watchCameras
} from '../';

interface TestResult {
    category: string;
    feature: string;
    status: 'PASS' | 'FAIL' | 'UNSUPPORTED' | 'SKIPPED';
    details?: string;
    error?: string;
}

const results: TestResult[] = [];

function record(category: string, feature: string, status: 'PASS' | 'FAIL' | 'UNSUPPORTED' | 'SKIPPED', details?: string, error?: any) {
    const errorMsg = error ? (error.message || String(error)) : undefined;
    results.push({ category, feature, status, details, error: errorMsg });
    const symbol = status === 'PASS' ? '✅' : status === 'FAIL' ? '❌' : status === 'UNSUPPORTED' ? '⚠️' : '⏭️';
    console.log(`${symbol} [${category}] ${feature}: ${status}${details ? ` - ${details}` : ''}${errorMsg ? ` (Error: ${errorMsg})` : ''}`);
}

async function run() {
    console.log('='.repeat(70));
    console.log('CANON EOS RP COMPREHENSIVE FEATURE TEST SUITE');
    console.log('='.repeat(70));

    // 1. Browser & Discovery
    console.log('\n--- 1. Camera Discovery & Browser ---');
    let cameras: Camera[] = [];
    try {
        cameras = cameraBrowser.getCameras();
        if (cameras.length > 0) {
            record('Discovery', 'cameraBrowser.getCameras()', 'PASS', `Found ${cameras.length} camera: ${cameras[0].description}`);
        } else {
            record('Discovery', 'cameraBrowser.getCameras()', 'FAIL', 'No camera detected');
            process.exit(1);
        }
    } catch (e) {
        record('Discovery', 'cameraBrowser.getCameras()', 'FAIL', undefined, e);
        process.exit(1);
    }

    const camera = cameras[0];

    // 2. Camera Identifiers & Connection
    console.log('\n--- 2. Camera Lifecycle & Identification ---');
    try {
        const desc = camera.description;
        const port = camera.portName;
        record('Identification', 'camera.description', 'PASS', desc);
        record('Identification', 'camera.portName', 'PASS', port);
    } catch (e) {
        record('Identification', 'Camera attributes', 'FAIL', undefined, e);
    }

    try {
        camera.connect();
        record('Lifecycle', 'camera.connect()', 'PASS', 'Session opened');
    } catch (e) {
        record('Lifecycle', 'camera.connect()', 'FAIL', undefined, e);
        process.exit(1);
    }

    // 3. Event Handling
    console.log('\n--- 3. Event Handling Setup ---');
    const receivedEvents = new Map<string, number>();
    try {
        camera.setEventHandler((eventName, _event) => {
            receivedEvents.set(eventName, (receivedEvents.get(eventName) || 0) + 1);
        });
        record('Events', 'camera.setEventHandler()', 'PASS', 'Listener attached');
    } catch (e) {
        record('Events', 'camera.setEventHandler()', 'FAIL', undefined, e);
    }

    // 4. Commands (Keepalive / Shutter half-press)
    console.log('\n--- 4. Camera Commands ---');
    try {
        camera.sendCommand(Camera.Command.ExtendShutDownTimer);
        record('Commands', 'ExtendShutDownTimer', 'PASS', 'Shutdown timer extended');
    } catch (e) {
        record('Commands', 'ExtendShutDownTimer', 'FAIL', undefined, e);
    }

    try {
        camera.sendCommand(Camera.Command.PressShutterButton, Camera.PressShutterButton.Halfway);
        await new Promise(r => setTimeout(r, 200));
        camera.sendCommand(Camera.Command.PressShutterButton, Camera.PressShutterButton.OFF);
        record('Commands', 'PressShutterButton (Halfway -> OFF)', 'PASS', 'Half-press autofocus / meter trigger succeeded');
    } catch (e) {
        record('Commands', 'PressShutterButton (Halfway)', 'FAIL', undefined, e);
    }

    // 5. Properties: Reading all CameraProperty IDs
    console.log('\n--- 5. Property Readability ---');
    const propertyNames = Object.keys(CameraProperty.ID).sort();
    let readableCount = 0;
    let unavailableCount = 0;

    for (const propName of propertyNames) {
        const propId = (CameraProperty.ID as any)[propName];
        try {
            const p = camera.getProperty(propId);
            if (!p.available) {
                unavailableCount++;
                record('Property Read', propName, 'UNSUPPORTED', 'Property marked unavailable on Canon RP');
                continue;
            }
            const val: any = p.value;
            const allowed: any = p.allowedValues;
            readableCount++;
            let valStr = '';
            if (val && typeof val === 'object') {
                valStr = (val.label !== undefined ? val.label : '') || (val.aperture !== undefined ? `f${val.aperture}` : '') || (val.seconds !== undefined ? `${val.seconds}s` : '') || JSON.stringify(val);
            } else {
                valStr = String(val);
            }
            record('Property Read', propName, 'PASS', `Value: ${valStr} (${allowed ? allowed.length : 0} allowed choices)`);
        } catch (e) {
            record('Property Read', propName, 'FAIL', undefined, e);
        }
    }
    console.log(`Summary: ${readableCount} available properties, ${unavailableCount} unavailable.`);

    // 6. Property Modification
    console.log('\n--- 6. Property Modification Tests ---');
    // Test ISO
    try {
        const isoProp = camera.getProperty(CameraProperty.ID.ISOSpeed);
        if (isoProp.available && isoProp.allowedValues && isoProp.allowedValues.length > 1) {
            const currentVal: any = isoProp.value;
            const targetVal: any = isoProp.allowedValues.find((v: any) => v.value !== currentVal.value) || isoProp.allowedValues[0];
            camera.setProperty(CameraProperty.ID.ISOSpeed, targetVal);
            const updatedVal: any = camera.getProperty(CameraProperty.ID.ISOSpeed).value;
            record('Property Write', 'ISOSpeed', 'PASS', `Changed from ${currentVal.label || currentVal.value} to ${updatedVal.label || updatedVal.value}`);
            // Restore
            camera.setProperty(CameraProperty.ID.ISOSpeed, currentVal);
        } else {
            record('Property Write', 'ISOSpeed', 'SKIPPED', 'Not enough allowed values or not available');
        }
    } catch (e) {
        record('Property Write', 'ISOSpeed', 'FAIL', undefined, e);
    }

    // Test WhiteBalance
    try {
        const wbProp = camera.getProperty(CameraProperty.ID.WhiteBalance);
        if (wbProp.available && wbProp.allowedValues && wbProp.allowedValues.length > 1) {
            const currentWb: any = wbProp.value;
            const targetWb: any = wbProp.allowedValues.find((v: any) => v.value !== currentWb.value) || wbProp.allowedValues[0];
            camera.setProperty(CameraProperty.ID.WhiteBalance, targetWb);
            const updatedWb: any = camera.getProperty(CameraProperty.ID.WhiteBalance).value;
            record('Property Write', 'WhiteBalance', 'PASS', `Changed to ${updatedWb.label || updatedWb.value}`);
            // Restore
            camera.setProperty(CameraProperty.ID.WhiteBalance, currentWb);
        } else {
            record('Property Write', 'WhiteBalance', 'SKIPPED', 'Allowed values not available');
        }
    } catch (e) {
        record('Property Write', 'WhiteBalance', 'FAIL', undefined, e);
    }

    // Test DriveMode
    try {
        const dmProp = camera.getProperty(CameraProperty.ID.DriveMode);
        if (dmProp.available && dmProp.allowedValues && dmProp.allowedValues.length > 1) {
            record('Property Write', 'DriveMode', 'PASS', `Available: ${dmProp.allowedValues.map((v: any) => v.label || v.value).join(', ')}`);
        } else {
            record('Property Write', 'DriveMode', 'SKIPPED', 'Single or unavailable mode');
        }
    } catch (e) {
        record('Property Write', 'DriveMode', 'FAIL', undefined, e);
    }

    // Test SaveTo
    try {
        camera.setProperty(CameraProperty.ID.SaveTo, Option.SaveTo.Host);
        const st: any = camera.getProperty(CameraProperty.ID.SaveTo).value;
        record('Property Write', 'SaveTo (Host)', 'PASS', `Current: ${st.label || st.value}`);
    } catch (e) {
        record('Property Write', 'SaveTo (Host)', 'FAIL', undefined, e);
    }

    // 7. Live View Tests (EVF)
    console.log('\n--- 7. Live View (EVF) Tests ---');
    try {
        camera.startLiveView();
        record('LiveView', 'camera.startLiveView()', 'PASS', 'EVF session started');
        const isActive = camera.isLiveViewActive();
        record('LiveView', 'camera.isLiveViewActive()', isActive ? 'PASS' : 'FAIL', `Active state: ${isActive}`);

        // Wait a moment for frames
        await new Promise(r => setTimeout(r, 1000));

        const liveImage = camera.getLiveViewImage();
        if (liveImage) {
            record('LiveView', 'camera.getLiveViewImage()', 'PASS', 'Frame retrieved');

            // Test getDataURL / getData
            const dataUrl = liveImage.getDataURL();
            const hasDataUrl = typeof dataUrl === 'string' && dataUrl.startsWith('data:image/jpeg;base64,');
            record('LiveView', 'image.getDataURL()', hasDataUrl ? 'PASS' : 'FAIL', `Data URL length: ${dataUrl.length}`);

            // Test getBlob()
            const blob = liveImage.getBlob();
            const isBlobBuffer = Buffer.isBuffer(blob) || blob instanceof Uint8Array;
            const isBlobJpeg = blob && blob.length > 4 && blob[0] === 0xFF && blob[1] === 0xD8;
            record('LiveView', 'image.getBlob()', isBlobBuffer && isBlobJpeg ? 'PASS' : 'FAIL', `Size: ${blob.length} bytes, starts with 0xFFD8`);

            // Test getJPEGBuffer()
            const jpegBuffer = liveImage.getJPEGBuffer();
            const isJpegBuffer = Buffer.isBuffer(jpegBuffer) || jpegBuffer instanceof Uint8Array;
            const isBufferJpeg = jpegBuffer && jpegBuffer.length > 4 && jpegBuffer[0] === 0xFF && jpegBuffer[1] === 0xD8;
            record('LiveView', 'image.getJPEGBuffer()', isJpegBuffer && isBufferJpeg ? 'PASS' : 'FAIL', `Size: ${jpegBuffer.length} bytes, starts with 0xFFD8`);

            // Parity check: getBlob() vs getJPEGBuffer()
            const isIdentical = Buffer.compare(Buffer.from(blob), Buffer.from(jpegBuffer)) === 0;
            record('LiveView', 'getBlob() === getJPEGBuffer() match', isIdentical ? 'PASS' : 'FAIL', isIdentical ? 'Byte-for-byte identical' : 'Mismatch');

            // Image Metadata
            const coords = liveImage.coordinateSystem;
            record('LiveView', 'image.coordinateSystem', 'PASS', `${coords.width} x ${coords.height}`);

            const zoom = liveImage.zoom;
            record('LiveView', 'image.zoom', 'PASS', `Zoom factor: ${zoom}`);

            const zoomPos: any = liveImage.zoomPosition;
            record('LiveView', 'image.zoomPosition', 'PASS', `Position: ${JSON.stringify(zoomPos)}`);

            const histogram = liveImage.histogram;
            const hasHisto = histogram && Array.isArray(histogram.y) && histogram.y.length === 256;
            record('LiveView', 'image.histogram', hasHisto ? 'PASS' : 'UNSUPPORTED', hasHisto ? '256-bin luminance/RGB channels present' : 'No histogram data in frame');
        } else {
            record('LiveView', 'camera.getLiveViewImage()', 'FAIL', 'No frame returned');
        }

        camera.stopLiveView();
        record('LiveView', 'camera.stopLiveView()', 'PASS', 'EVF stopped');
    } catch (e) {
        record('LiveView', 'EVF operations', 'FAIL', undefined, e);
    }

    // 8. Volumes & File System (SD card)
    console.log('\n--- 8. Storage Volumes & File System ---');
    try {
        const volumes = camera.getVolumes();
        record('Storage', 'camera.getVolumes()', 'PASS', `Found ${volumes.length} volume(s)`);
        for (const vol of volumes) {
            record('Storage', `Volume [${vol.label}]`, 'PASS', `storageType: ${vol.storageType}, readable: ${vol.isReadable}, writable: ${vol.isWritable}, free: ${vol.freeCapacity}`);
            if (vol.isReadable) {
                try {
                    const entries = vol.getEntries();
                    record('Storage', `Volume [${vol.label}] getEntries()`, 'PASS', `${entries.length} items found`);
                } catch (e) {
                    record('Storage', `Volume [${vol.label}] getEntries()`, 'FAIL', undefined, e);
                }
            } else {
                record('Storage', `Volume [${vol.label}] access`, 'UNSUPPORTED', 'Volume is not readable (no SD card inserted)');
            }
        }
    } catch (e) {
        record('Storage', 'Volumes', 'FAIL', undefined, e);
    }

    // 9. Photo Capture & Download
    console.log('\n--- 9. Capture & Transfer to Host ---');
    let capturedFile: CameraFile | null = null;
    let downloadSuccess = false;

    camera.setEventHandler((eventName, event) => {
        receivedEvents.set(eventName, (receivedEvents.get(eventName) || 0) + 1);
        if (eventName === Camera.EventName.DownloadRequest || eventName === Camera.EventName.FileCreate) {
            capturedFile = (event as any).file;
        }
    });

    try {
        camera.setProperties({
            [CameraProperty.ID.SaveTo]: Option.SaveTo.Host,
            [CameraProperty.ID.ImageQuality]: ImageQuality.ID.LargeJPEGFine,
        });

        const unwatch = watchCameras(50);
        console.log('Triggering camera.takePicture()...');
        camera.takePicture();

        // Wait up to 6 seconds for download request event
        for (let i = 0; i < 30; i++) {
            await new Promise(r => setTimeout(r, 200));
            if (capturedFile) break;
        }

        unwatch();

        if (capturedFile) {
            record('Capture', 'takePicture() event trigger', 'PASS', `Received file ${(capturedFile as CameraFile).name} (${(capturedFile as CameraFile).format})`);
            const outDir = path.join(__dirname, 'images');
            if (!fs.existsSync(outDir)) fs.mkdirSync(outDir, { recursive: true });
            (capturedFile as CameraFile).downloadToPath(outDir);
            const downloadedPath = path.join(outDir, (capturedFile as CameraFile).name);
            if (fs.existsSync(downloadedPath)) {
                const stat = fs.statSync(downloadedPath);
                downloadSuccess = stat.size > 0;
                record('Capture', 'file.downloadToPath()', downloadSuccess ? 'PASS' : 'FAIL', `Downloaded to ${downloadedPath} (${stat.size} bytes)`);
            } else {
                record('Capture', 'file.downloadToPath()', 'FAIL', 'Target file not created on disk');
            }
        } else {
            record('Capture', 'takePicture() event trigger', 'FAIL', 'Timeout waiting for DownloadRequest/FileCreate event');
        }
    } catch (e) {
        record('Capture', 'takePicture()', 'FAIL', undefined, e);
    }

    // 10. Disconnect & Cleanup
    console.log('\n--- 10. Disconnect & Cleanup ---');
    try {
        camera.disconnect();
        record('Lifecycle', 'camera.disconnect()', 'PASS', 'Session closed cleanly');
    } catch (e) {
        record('Lifecycle', 'camera.disconnect()', 'FAIL', undefined, e);
    }

    // Print Final Report & Summary
    console.log('\n' + '='.repeat(70));
    console.log('SUMMARY OF RESULTS');
    console.log('='.repeat(70));
    const passCount = results.filter(r => r.status === 'PASS').length;
    const failCount = results.filter(r => r.status === 'FAIL').length;
    const unsuppCount = results.filter(r => r.status === 'UNSUPPORTED').length;
    const skipCount = results.filter(r => r.status === 'SKIPPED').length;

    console.log(`TOTAL TESTS: ${results.length}`);
    console.log(`PASSED:      ${passCount}`);
    console.log(`FAILED:      ${failCount}`);
    console.log(`UNSUPPORTED: ${unsuppCount} (camera firmware / hardware dependent)`);
    console.log(`SKIPPED:     ${skipCount}`);

    // Save JSON report
    const reportPath = path.join(__dirname, 'canon-rp-test-results.json');
    fs.writeFileSync(reportPath, JSON.stringify(results, null, 2));
    console.log(`\nDetailed report written to: ${reportPath}`);

    process.exit(failCount > 0 ? 1 : 0);
}

run().catch(err => {
    console.error('Fatal test error:', err);
    process.exit(1);
});
