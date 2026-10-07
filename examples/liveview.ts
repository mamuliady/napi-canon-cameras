/**
 * LiveView Example - Optimized for smooth frame capture
 * 
 * This example demonstrates optimal LiveView usage with:
 * - setImmediate loop for better performance (15-25 fps)
 * - Frame timing statistics
 * - Proper error handling and cleanup
 * 
 * For basic example, see the bottom of this file (commented out)
 */

import { cameraBrowser, CameraBrowser, Camera, CameraProperty, LiveViewImage, watchCameras } from '../';
import { EventEmitter } from 'events';
import * as fs from 'fs';
import * as http from 'http';
import * as path from 'path';

const events = new EventEmitter();
events.on(
    CameraBrowser.EventName.PropertyChangeValue,
    (event) => {
        if (event.property.identifier === CameraProperty.ID.Evf_OutputDevice) {
            console.log(event.property.label, event.property.value);
        }
    },
);
events.on(
    CameraBrowser.EventName.LiveViewStart,
    (event) => {
        console.log('LiveView Started', event.camera);
    },
);
events.on(
    CameraBrowser.EventName.LiveViewStop,
    (event) => {
        console.log('LiveView Stopped', event.camera);
    },
);

cameraBrowser.setEventHandler(
    (eventName, ...args) => {
        events.emit(eventName, ...args);
    },
);

// Global cleanup handler will be set in main execution

/**
 * Optimized LiveView capture using setImmediate
 * Achieves 15-25 fps typical performance
 */
function runOptimizedLiveView(camera: Camera) {
    console.log('\n=== Starting Optimized LiveView ===');
    console.log('Using setImmediate loop for better performance');
    console.log('Press Ctrl+C to stop\n');
    
    camera.startLiveView();
    
    let frameCount = 0;
    let shouldStop = false;
    const startTime = Date.now();
    let lastFrameTime = startTime;
    let lastLogTime = startTime;
    
    // Create output directory for sample images
    const outputDir = path.join(__dirname, 'images', 'liveview');
    if (!fs.existsSync(outputDir)) {
        fs.mkdirSync(outputDir, { recursive: true });
    }
    
    const captureFrame = () => {
        if (shouldStop) {
            return;
        }
        
        const now = Date.now();
        const deltaTime = now - lastFrameTime;
        
        try {
            const image: LiveViewImage = camera.getLiveViewImage();
            
            if (image) {
                frameCount++;
                lastFrameTime = now;
                
                // Log stats every second
                if (now - lastLogTime >= 1000) {
                    const elapsed = (now - startTime) / 1000;
                    const currentFps = frameCount / elapsed;
                    const coords = image.coordinateSystem;
                    console.log(
                        `Frame ${frameCount}: ${currentFps.toFixed(1)} fps avg | ` +
                        `${coords.width}x${coords.height} | ` +
                        `Zoom: ${image.zoom.label} | ` +
                        `Delta: ${deltaTime}ms`
                    );
                    lastLogTime = now;
                }
                
                // Save every 60th frame as sample
                if (frameCount % 60 === 0) {
                    const buffer = image.getBlob();
                    const filename = path.join(outputDir, `frame-${frameCount}.jpg`);
                    fs.writeFileSync(filename, buffer);
                    console.log(`  → Saved: ${filename}`);
                }
            }
            
            // Non-blocking continuation
            setImmediate(captureFrame);
            
        } catch (e) {
            console.error('Frame capture error:', e);
            // Continue despite errors
            setImmediate(captureFrame);
        }
    };
    
    // Start capture loop
    setImmediate(captureFrame);
    
    // Cleanup handler
    const cleanup = () => {
        if (!shouldStop) {
            shouldStop = true;
            
            try {
                camera.stopLiveView();
                
                const totalTime = (Date.now() - startTime) / 1000;
                const avgFps = frameCount / totalTime;
                
                console.log('\n=== LiveView Statistics ===');
                console.log(`Total Frames: ${frameCount}`);
                console.log(`Total Time: ${totalTime.toFixed(2)}s`);
                console.log(`Average FPS: ${avgFps.toFixed(1)}`);
                console.log(`Sample images saved to: ${outputDir}`);
            } catch (e) {
                console.error('Cleanup error:', e);
            }
        }
    };
    
    process.on('SIGINT', cleanup);
    process.on('exit', cleanup);
    
    // Cleanup on exit
    const exitCleanup = () => {
        if (!shouldStop) {
            shouldStop = true;
            try {
                camera.stopLiveView();
                camera.disconnect();
                cameraBrowser.terminate();
            } catch (e) {
                // Ignore cleanup errors
            }
        }
    };
    
    process.on('SIGINT', exitCleanup);
    process.on('SIGTERM', exitCleanup);
    
    // Optional: Auto-stop after duration (comment out for continuous)
    // setTimeout(() => {
    //     cleanup();
    //     process.exit(0);
    // }, 30000); // 30 seconds
}

/**
 * HTTP stream server for browser viewing
 * Access at http://localhost:8080
 */
function runLiveViewWebStream(camera: Camera) {
    console.log('\n=== Starting LiveView Web Stream ===');
    
    camera.startLiveView();
    
    const server = http.createServer((req: http.IncomingMessage, res: http.ServerResponse) => {
        if (req.url === '/stream') {
            res.writeHead(200, {
                'Content-Type': 'multipart/x-mixed-replace; boundary=frame',
                'Cache-Control': 'no-cache',
                'Connection': 'keep-alive',
                'Access-Control-Allow-Origin': '*'
            });
            
            let shouldStream = true;
            
            const sendFrame = () => {
                if (!shouldStream || res.destroyed) {
                    return;
                }
                
                try {
                    const image = camera.getLiveViewImage();
                    if (image) {
                        // getDataURL() returns a base64 data URL string like "data:image/jpeg;base64,..."
                        // Extract the base64 part and convert to Buffer
                        const dataURL = image.getDataURL();
                        const base64Data = dataURL.replace(/^data:image\/jpeg;base64,/, '');
                        const jpegBuffer = Buffer.from(base64Data, 'base64');
                        
                        // Send as multipart/x-mixed-replace boundary
                        res.write(Buffer.from(
                            '--frame\r\n' +
                            'Content-Type: image/jpeg\r\n' +
                            `Content-Length: ${jpegBuffer.length}\r\n\r\n`
                        ));
                        res.write(jpegBuffer);
                        res.write(Buffer.from('\r\n'));
                    }
                } catch (e) {
                    // Silently ignore OBJECT_NOTREADY errors - they're normal
                    const error = e as Error;
                    if (!error.message?.includes('OBJECT_NOTREADY')) {
                        console.error('Stream error:', error.message);
                    }
                }
                
                // Small delay to prevent hammering the camera (~30fps)
                setTimeout(sendFrame, 33);
            };
            
            sendFrame();
            
            req.on('close', () => {
                shouldStream = false;
                console.log('Client disconnected');
            });
            
        } else if (req.url === '/') {
            res.writeHead(200, { 'Content-Type': 'text/html' });
            res.end(`
                <!DOCTYPE html>
                <html>
                <head>
                    <title>Canon Camera LiveView</title>
                    <style>
                        body { 
                            margin: 0; 
                            background: #000; 
                            display: flex; 
                            flex-direction: column;
                            justify-content: center; 
                            align-items: center; 
                            height: 100vh;
                            font-family: Arial, sans-serif;
                        }
                        h1 {
                            color: #fff;
                            margin-bottom: 20px;
                        }
                        img { 
                            max-width: 90%; 
                            max-height: 80vh; 
                            border: 2px solid #333;
                        }
                    </style>
                </head>
                <body>
                    <h1>Canon Camera LiveView</h1>
                    <img src="/stream" alt="LiveView Stream">
                </body>
                </html>
            `);
        } else {
            res.writeHead(404);
            res.end('Not Found');
        }
    });
    
    server.listen(8080, () => {
        console.log('LiveView stream server started!');
        console.log('  → Open browser: http://localhost:8080');
        console.log('  → Stream URL: http://localhost:8080/stream');
        console.log('\nPress Ctrl+C to stop');
    });
    
    const cleanup = () => {
        console.log('\nStopping server...');
        server.close();
        try {
            camera.stopLiveView();
            camera.disconnect();
            cameraBrowser.terminate();
        } catch (e) {
            console.error('Cleanup error:', (e as Error).message);
        }
        process.exit(0);
    };
    
    process.on('SIGINT', cleanup);
    process.on('SIGTERM', cleanup);
}

// Main execution
try {
    const camera = cameraBrowser.getCamera();
    if (!camera) {
        console.error('No camera found!');
        process.exit(1);
    }
    
    console.log('Found camera:', camera.description);
    camera.connect();
    console.log('Camera connected');
    
    if (!camera.getProperty(CameraProperty.ID.Evf_Mode).available) {
        console.error('LiveView not available on this camera');
        camera.disconnect();
        process.exit(1);
    }
    
    // Choose which mode to run:
    // 1. Optimized capture with statistics (default)
    // runOptimizedLiveView(camera);
    
    // 2. Web browser stream (uncomment to use)
    runLiveViewWebStream(camera);
    
    watchCameras();
    
} catch (e) {
    console.error('Error:', e);
    process.exit(1);
}

/* ========================================
 * BASIC EXAMPLE (Original Implementation)
 * ========================================
 * This is the original simple approach using setInterval.
 * Performance: 5-15 fps typical
 * Kept here for reference.

try {
    const camera = cameraBrowser.getCamera();
    if (camera) {
        console.log(camera);
        camera.connect();
        console.log();
    }
    let liveMode = false;

    if (camera.getProperty(CameraProperty.ID.Evf_Mode).available) {
        camera.startLiveView();
        
        // Toggle LiveView on/off every 5 seconds
        setInterval(
            () => {
                if (liveMode) {
                    camera.startLiveView();
                } else {
                    camera.stopLiveView();
                }
                liveMode = !liveMode;
            },
            5000,
        );
        
        // Capture frames every 200ms (5fps target)
        setInterval(
            () => {
                try {
                    const image = camera.getLiveViewImage();
                    if (image) {
                        console.log(
                            {
                                image: image.getDataURL().substring(0, 40),
                            },
                        );
                    }
                } catch (e) {
                    console.log(e);
                }
            },
            200,
        );
    }

    watchCameras();
} catch (e) {
    console.log(e);
}

*/
