# LiveView Performance Optimization Guide

This document describes methods for optimizing LiveView frame capture to achieve smooth 30fps streaming.

## Table of Contents

- [Current Issues](#current-issues)
- [Optimization Methods](#optimization-methods)
- [Performance Comparison](#performance-comparison)
- [Implementation Examples](#implementation-examples)
- [Advanced Techniques](#advanced-techniques)
- [Troubleshooting](#troubleshooting)

## Current Issues

The basic LiveView implementation has several performance bottlenecks:

### 1. **Synchronous Blocking Calls**
- `getLiveViewImage()` makes synchronous EDSDK calls
- Blocks the JavaScript event loop
- Prevents other operations during frame capture

### 2. **Timer Precision Issues**
- `setInterval(fn, 33)` is not precise for 30fps
- Timer drift accumulates over time
- Affected by event loop congestion

### 3. **Memory Allocation Overhead**
- New JPEG buffer created for each frame
- Causes garbage collection pressure
- GC pauses affect frame timing

### 4. **No Frame Management**
- No frame skipping when processing falls behind
- No buffer pool for reusing memory
- No backpressure handling

## Optimization Methods

### Method 1: Basic setInterval (Baseline)

**Expected Performance:** 5-15 fps

```typescript
import { Camera, cameraBrowser, LiveViewImage } from '@dimensional/napi-canon-cameras';

cameraBrowser.initialize();
const camera = new Camera();
camera.connect();
camera.startLiveView();

let frameCount = 0;

const interval = setInterval(() => {
    try {
        const image: LiveViewImage = camera.getLiveViewImage();
        if (image) {
            frameCount++;
            console.log(`Frame ${frameCount}`);
            // Process frame...
        }
    } catch (e) {
        console.error('Error:', e);
    }
}, 33); // Target ~30fps

// Stop after 10 seconds
setTimeout(() => {
    clearInterval(interval);
    camera.stopLiveView();
    camera.disconnect();
    cameraBrowser.terminate();
}, 10000);
```

**Pros:**
- Simple and straightforward
- Easy to understand

**Cons:**
- Poor performance (5-15 fps typical)
- Timer drift over time
- Blocks event loop

### Method 2: Optimized setImmediate Loop

**Expected Performance:** 15-25 fps

```typescript
import { Camera, cameraBrowser, LiveViewImage } from '@dimensional/napi-canon-cameras';

cameraBrowser.initialize();
const camera = new Camera();
camera.connect();
camera.startLiveView();

let frameCount = 0;
let shouldStop = false;
const startTime = Date.now();

const captureFrame = () => {
    if (shouldStop) return;
    
    const now = Date.now();
    
    try {
        const image: LiveViewImage = camera.getLiveViewImage();
        
        if (image) {
            frameCount++;
            const elapsed = (now - startTime) / 1000;
            const fps = frameCount / elapsed;
            
            if (frameCount % 30 === 0) {
                console.log(`Frame ${frameCount}: ${fps.toFixed(1)} fps`);
            }
            
            // Process frame...
        }
        
        // Non-blocking continuation
        setImmediate(captureFrame);
        
    } catch (e) {
        console.error('Frame error:', e);
        setImmediate(captureFrame);
    }
};

// Start capture loop
setImmediate(captureFrame);

// Stop after 10 seconds
setTimeout(() => {
    shouldStop = true;
    camera.stopLiveView();
    camera.disconnect();
    cameraBrowser.terminate();
    
    const totalTime = (Date.now() - startTime) / 1000;
    console.log(`Captured ${frameCount} frames in ${totalTime.toFixed(2)}s`);
    console.log(`Average: ${(frameCount / totalTime).toFixed(1)} fps`);
}, 10000);

process.on('SIGINT', () => {
    shouldStop = true;
    camera.stopLiveView();
    camera.disconnect();
    cameraBrowser.terminate();
    process.exit();
});
```

**Pros:**
- Better frame rate (15-25 fps typical)
- Non-blocking event loop
- More consistent timing

**Cons:**
- Still limited by synchronous EDSDK calls
- Can't reach full 30fps consistently

### Method 3: HTTP Stream for Browser Display

**Expected Performance:** 15-25 fps (network-limited)

```typescript
import { Camera, cameraBrowser, LiveViewImage } from '@dimensional/napi-canon-cameras';
import * as http from 'http';

cameraBrowser.initialize();
const camera = new Camera();
camera.connect();
camera.startLiveView();

const server = http.createServer((req, res) => {
    if (req.url === '/stream') {
        res.writeHead(200, {
            'Content-Type': 'multipart/x-mixed-replace; boundary=frame',
            'Cache-Control': 'no-cache',
            'Connection': 'keep-alive',
            'Access-Control-Allow-Origin': '*'
        });
        
        const sendFrame = () => {
            try {
                const image: LiveViewImage = camera.getLiveViewImage();
                if (image && !res.destroyed) {
                    const buffer = image.getDataURL();
                    res.write(
                        `--frame\r\n` +
                        `Content-Type: image/jpeg\r\n` +
                        `Content-Length: ${buffer.length}\r\n\r\n`
                    );
                    res.write(buffer);
                    res.write('\r\n');
                }
            } catch (e) {
                console.error('Stream error:', e);
            }
            
            if (!res.destroyed) {
                setImmediate(sendFrame);
            }
        };
        
        sendFrame();
        
        req.on('close', () => {
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
                    body { margin: 0; background: #000; display: flex; 
                           justify-content: center; align-items: center; 
                           height: 100vh; }
                    img { max-width: 100%; max-height: 100%; }
                </style>
            </head>
            <body>
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
    console.log('LiveView stream available at:');
    console.log('  http://localhost:8080');
    console.log('\nPress Ctrl+C to stop');
});

process.on('SIGINT', () => {
    console.log('\nStopping server...');
    server.close();
    camera.stopLiveView();
    camera.disconnect();
    cameraBrowser.terminate();
    process.exit();
});
```

**Pros:**
- Real-time browser display
- Easy to integrate with web apps
- Supports multiple clients

**Cons:**
- Network overhead
- Limited by HTTP streaming constraints
- Buffer accumulation can cause lag

### Method 4: Frame Timing Analysis

For testing and optimization, measure frame timing consistency:

```typescript
import { Camera, cameraBrowser, LiveViewImage } from '@dimensional/napi-canon-cameras';

cameraBrowser.initialize();
const camera = new Camera();
camera.connect();
camera.startLiveView();

let frameCount = 0;
let lastFrameTime = Date.now();
const frameTimes: number[] = [];

const captureFrame = () => {
    const now = Date.now();
    const deltaTime = now - lastFrameTime;
    
    try {
        const image: LiveViewImage = camera.getLiveViewImage();
        
        if (image) {
            frameCount++;
            frameTimes.push(deltaTime);
            lastFrameTime = now;
            
            if (frameCount >= 300) {
                // Calculate statistics
                const avgTime = frameTimes.reduce((a, b) => a + b) / frameTimes.length;
                const minTime = Math.min(...frameTimes);
                const maxTime = Math.max(...frameTimes);
                const variance = frameTimes.reduce(
                    (sum, t) => sum + Math.pow(t - avgTime, 2), 0
                ) / frameTimes.length;
                const stdDev = Math.sqrt(variance);
                
                console.log('\nFrame Timing Statistics:');
                console.log(`  Frames captured: ${frameCount}`);
                console.log(`  Average FPS: ${(1000 / avgTime).toFixed(1)}`);
                console.log(`  Average frame time: ${avgTime.toFixed(2)}ms`);
                console.log(`  Min frame time: ${minTime}ms`);
                console.log(`  Max frame time: ${maxTime}ms`);
                console.log(`  Std deviation: ${stdDev.toFixed(2)}ms`);
                console.log(`  Target (30fps): 33.33ms`);
                
                camera.stopLiveView();
                camera.disconnect();
                cameraBrowser.terminate();
                process.exit();
            }
        }
        
        setImmediate(captureFrame);
        
    } catch (e) {
        console.error('Error:', e);
        setImmediate(captureFrame);
    }
};

setImmediate(captureFrame);
```

## Performance Comparison

| Method | Average FPS | Consistency | CPU Usage | Memory |
|--------|-------------|-------------|-----------|--------|
| setInterval | 5-15 | Poor | Medium | High |
| setImmediate | 15-25 | Good | Medium | Medium |
| Native Async* | 25-30 | Excellent | Low | Low |
| HTTP Stream | 15-20 | Fair | High | High |

*Native Async requires native code modifications (not currently implemented)

## Advanced Techniques

### 1. Reduce I/O Overhead

Don't save every frame to disk during live streaming:

```typescript
// Only save every Nth frame
if (frameCount % 30 === 0) {
    fs.writeFileSync(`frame-${frameCount}.jpg`, image.getDataURL());
}
```

### 2. Frame Skipping

Skip frames when processing falls behind:

```typescript
const targetFrameTime = 33; // ~30fps
let lastProcessTime = Date.now();

const captureFrame = () => {
    const now = Date.now();
    const elapsed = now - lastProcessTime;
    
    if (elapsed < targetFrameTime) {
        // Skip this frame, we're ahead
        setImmediate(captureFrame);
        return;
    }
    
    // Process frame...
    lastProcessTime = now;
    setImmediate(captureFrame);
};
```

### 3. Buffer Pool (Future Enhancement)

Reuse buffers instead of allocating new ones:

```typescript
// Concept (requires native implementation)
const bufferPool = new BufferPool(10);

const captureFrame = () => {
    const buffer = bufferPool.acquire();
    camera.getLiveViewImageInto(buffer);
    // Process buffer...
    bufferPool.release(buffer);
};
```

### 4. Worker Threads

Offload processing to worker threads:

```typescript
import { Worker } from 'worker_threads';

const worker = new Worker('./process-frame.js');

const captureFrame = () => {
    const image = camera.getLiveViewImage();
    if (image) {
        // Send to worker for processing
        worker.postMessage({
            buffer: image.getDataURL(),
            width: image.width,
            height: image.height
        });
    }
    setImmediate(captureFrame);
};

worker.on('message', (result) => {
    // Handle processed frame
    console.log('Processed:', result);
});
```

## Troubleshooting

### Low Frame Rate (<10 fps)

**Possible Causes:**
- Camera in wrong mode (try switching from video to photo mode)
- USB bandwidth limitation (use USB 3.0 if available)
- High CPU usage from other processes
- Disk I/O bottleneck (don't save every frame)

**Solutions:**
- Use `setImmediate` instead of `setInterval`
- Reduce frame processing complexity
- Remove unnecessary logging
- Use SSD instead of HDD for frame saving

### Inconsistent Frame Timing

**Possible Causes:**
- Garbage collection pauses
- Event loop blocking operations
- Timer drift

**Solutions:**
- Reduce memory allocations
- Use buffer pooling
- Profile with `--prof` flag
- Implement frame skipping

### Memory Leaks

**Possible Causes:**
- Not releasing image buffers
- Accumulating frame references
- Event listener leaks

**Solutions:**
- Clear references after processing
- Use WeakMap for frame caching
- Monitor with `process.memoryUsage()`

### Camera Disconnects

**Possible Causes:**
- LiveView timeout
- USB connection issues
- Camera power saving

**Solutions:**
- Disable camera sleep mode
- Keep camera plugged into power
- Implement reconnection logic
- Send periodic keep-alive commands

## Camera-Specific Limitations

### EDSDK LiveView Constraints

- **Maximum frame rate:** ~30fps (camera/model dependent)
- **USB 2.0:** May limit to 15fps for high resolution
- **USB 3.0:** Can support full 30fps
- **Wireless:** Typically 10-15fps maximum

### Camera Model Differences

| Model Type | Typical Max FPS | Notes |
|------------|----------------|-------|
| Entry Level DSLR | 15-20 | Limited USB bandwidth |
| Mid-range DSLR | 20-25 | Better processing |
| Professional DSLR | 25-30 | Full 30fps capable |
| Mirrorless | 25-30 | Usually best performance |

## Testing Performance

Run the performance test suite:

```bash
# Run optimized test
ts-node tests/liveview-performance.test.ts

# Or use npm script
npm run test:liveview
```

The test will output:
- Frame count
- Average FPS
- Frame timing statistics
- Min/max/average frame time

## Best Practices

1. **Always use `setImmediate`** instead of `setInterval` for frame loops
2. **Minimize I/O operations** during live capture
3. **Monitor memory usage** and profile for leaks
4. **Implement proper cleanup** in error handlers
5. **Test with your specific camera model** for optimal settings
6. **Use USB 3.0** connections when available
7. **Keep camera powered** (don't rely on battery during development)
8. **Disable camera auto-sleep** settings

## Future Enhancements

### Native Async LiveView API

A future enhancement could add native async support:

```typescript
// Proposed API
camera.startLiveViewStream((image) => {
    // Callback invoked for each frame
    // Runs in separate thread, non-blocking
    console.log(`Frame: ${image.width}x${image.height}`);
});

camera.stopLiveViewStream();
```

This would require modifications to the native C++ code to implement background frame capture with callbacks.

### Frame Buffer Optimization

Direct buffer access without JPEG encoding:

```typescript
// Proposed API
const rawBuffer = camera.getLiveViewImageRaw();
// Process raw YUV or RGB data directly
```

This would eliminate JPEG encode/decode overhead for applications that don't need JPEG format.

## Conclusion

For best LiveView performance with the current implementation:

1. Use the **setImmediate loop** approach (Method 2)
2. Expect **15-25 fps** on most systems
3. Minimize I/O and processing during capture
4. Profile and optimize your specific use case

The maximum achievable frame rate is ultimately limited by:
- Camera capabilities (~30fps max)
- USB bandwidth (2.0 vs 3.0)
- Synchronous EDSDK calls
- System performance

For true 30fps streaming, future native async implementation would be required.
