/**
 * LiveView Performance Test
 * Tests various methods for capturing LiveView frames at optimal frame rates
 */

import { Camera, cameraBrowser } from '../';
import * as fs from 'fs';
import * as path from 'path';

// Create output directory for test images
const outputDir = path.join(__dirname, '../examples/images/liveview-test');
if (!fs.existsSync(outputDir)) {
    fs.mkdirSync(outputDir, { recursive: true });
}

/**
 * Test 1: Basic setInterval approach (NOT OPTIMAL)
 * Expected: 5-15 fps
 */
export function testBasicInterval() {
    console.log('\n=== Test 1: Basic setInterval (Baseline) ===');
    
    cameraBrowser.initialize();
    const cameras = cameraBrowser.getCameras();
    
    if (cameras.length === 0) {
        console.log('No cameras detected!');
        return;
    }
    
    const camera = cameras[0];
    camera.connect();
    console.log('Camera connected');
    
    camera.startLiveView();
    console.log('LiveView started');
    
    let frameCount = 0;
    const maxFrames = 100;
    const startTime = Date.now();
    
    const interval = setInterval(() => {
        try {
            const image: LiveViewImage = camera.getLiveViewImage();
            
            if (image) {
                frameCount++;
                
                if (frameCount === 1 || frameCount % 30 === 0) {
                    const elapsed = (Date.now() - startTime) / 1000;
                    const fps = frameCount / elapsed;
                    console.log(`Frame ${frameCount}: ${fps.toFixed(1)} fps`);
                }
                
                if (frameCount >= maxFrames) {
                    clearInterval(interval);
                    const totalTime = (Date.now() - startTime) / 1000;
                    const avgFps = frameCount / totalTime;
                    
                    console.log('\nTest Complete:');
                    console.log(`  Total Frames: ${frameCount}`);
                    console.log(`  Total Time: ${totalTime.toFixed(2)}s`);
                    console.log(`  Average FPS: ${avgFps.toFixed(1)}`);
                    
                    camera.stopLiveView();
                    camera.disconnect();
                    cameraBrowser.terminate();
                }
            }
        } catch (e) {
            console.error('Error capturing frame:', e);
        }
    }, 33); // Target ~30fps
}

/**
 * Test 2: Optimized setImmediate approach
 * Expected: 15-25 fps
 */
export function testOptimizedImmediate() {
    console.log('\n=== Test 2: Optimized setImmediate ===');
    
    cameraBrowser.initialize();
    const cameras = cameraBrowser.getCameras();
    
    if (cameras.length === 0) {
        console.log('No cameras detected!');
        return;
    }
    
    const camera = cameras[0];
    camera.connect();
    console.log('Camera connected');
    
    camera.startLiveView();
    console.log('LiveView started');
    
    let frameCount = 0;
    const maxFrames = 100;
    const startTime = Date.now();
    let lastFrameTime = startTime;
    let shouldStop = false;
    
    const captureFrame = () => {
        if (shouldStop) return;
        
        const now = Date.now();
        const deltaTime = now - lastFrameTime;
        
        try {
            const image: LiveViewImage = camera.getLiveViewImage();
            
            if (image) {
                frameCount++;
                lastFrameTime = now;
                
                if (frameCount === 1 || frameCount % 30 === 0) {
                    const elapsed = (now - startTime) / 1000;
                    const fps = frameCount / elapsed;
                    console.log(`Frame ${frameCount}: ${fps.toFixed(1)} fps avg, ${deltaTime}ms delta`);
                }
                
                if (frameCount >= maxFrames) {
                    shouldStop = true;
                    const totalTime = (now - startTime) / 1000;
                    const avgFps = frameCount / totalTime;
                    
                    console.log('\nTest Complete:');
                    console.log(`  Total Frames: ${frameCount}`);
                    console.log(`  Total Time: ${totalTime.toFixed(2)}s`);
                    console.log(`  Average FPS: ${avgFps.toFixed(1)}`);
                    
                    camera.stopLiveView();
                    camera.disconnect();
                    cameraBrowser.terminate();
                    return;
                }
            }
            
            // Non-blocking loop
            setImmediate(captureFrame);
            
        } catch (e) {
            console.error('Frame capture error:', e);
            setImmediate(captureFrame);
        }
    };
    
    // Start capture loop
    setImmediate(captureFrame);
    
    // Safety timeout
    setTimeout(() => {
        if (!shouldStop) {
            shouldStop = true;
            camera.stopLiveView();
            camera.disconnect();
            cameraBrowser.terminate();
        }
    }, 15000);
}

/**
 * Test 3: Performance with frame saving
 * Tests impact of I/O operations on frame rate
 */
export function testWithFrameSaving() {
    console.log('\n=== Test 3: With Frame Saving (I/O Impact) ===');
    
    cameraBrowser.initialize();
    const cameras = cameraBrowser.getCameras();
    
    if (cameras.length === 0) {
        console.log('No cameras detected!');
        return;
    }
    
    const camera = cameras[0];
    camera.connect();
    console.log('Camera connected');
    
    camera.startLiveView();
    console.log('LiveView started');
    
    let frameCount = 0;
    const maxFrames = 100;
    const startTime = Date.now();
    let shouldStop = false;
    
    const captureFrame = () => {
        if (shouldStop) return;
        
        const now = Date.now();
        
        try {
            const image: LiveViewImage = camera.getLiveViewImage();
            
            if (image) {
                frameCount++;
                
                // Save every 30th frame to test I/O impact
                if (frameCount % 30 === 0) {
                    const buffer = image.getDataURL();
                    fs.writeFileSync(
                        path.join(outputDir, `frame-${frameCount}.jpg`),
                        buffer
                    );
                    
                    const elapsed = (now - startTime) / 1000;
                    const fps = frameCount / elapsed;
                    console.log(`Frame ${frameCount}: ${fps.toFixed(1)} fps (saved to disk)`);
                }
                
                if (frameCount >= maxFrames) {
                    shouldStop = true;
                    const totalTime = (now - startTime) / 1000;
                    const avgFps = frameCount / totalTime;
                    
                    console.log('\nTest Complete:');
                    console.log(`  Total Frames: ${frameCount}`);
                    console.log(`  Saved Frames: ${Math.floor(frameCount / 30)}`);
                    console.log(`  Total Time: ${totalTime.toFixed(2)}s`);
                    console.log(`  Average FPS: ${avgFps.toFixed(1)}`);
                    
                    camera.stopLiveView();
                    camera.disconnect();
                    cameraBrowser.terminate();
                    return;
                }
            }
            
            setImmediate(captureFrame);
            
        } catch (e) {
            console.error('Frame capture error:', e);
            setImmediate(captureFrame);
        }
    };
    
    setImmediate(captureFrame);
    
    setTimeout(() => {
        if (!shouldStop) {
            shouldStop = true;
            camera.stopLiveView();
            camera.disconnect();
            cameraBrowser.terminate();
        }
    }, 15000);
}

/**
 * Test 4: Frame timing analysis
 * Measures frame-to-frame timing consistency
 */
export function testFrameTimingAnalysis() {
    console.log('\n=== Test 4: Frame Timing Analysis ===');
    
    cameraBrowser.initialize();
    const cameras = cameraBrowser.getCameras();
    
    if (cameras.length === 0) {
        console.log('No cameras detected!');
        return;
    }
    
    const camera = cameras[0];
    camera.connect();
    console.log('Camera connected');
    
    camera.startLiveView();
    console.log('LiveView started');
    
    let frameCount = 0;
    const maxFrames = 300;
    const startTime = Date.now();
    let lastFrameTime = startTime;
    let shouldStop = false;
    const frameTimes: number[] = [];
    
    const captureFrame = () => {
        if (shouldStop) return;
        
        const now = Date.now();
        const deltaTime = now - lastFrameTime;
        
        try {
            const image: LiveViewImage = camera.getLiveViewImage();
            
            if (image) {
                frameCount++;
                frameTimes.push(deltaTime);
                lastFrameTime = now;
                
                if (frameCount >= maxFrames) {
                    shouldStop = true;
                    const totalTime = (now - startTime) / 1000;
                    const avgFps = frameCount / totalTime;
                    
                    // Calculate statistics
                    const avgFrameTime = frameTimes.reduce((a, b) => a + b, 0) / frameTimes.length;
                    const minFrameTime = Math.min(...frameTimes);
                    const maxFrameTime = Math.max(...frameTimes);
                    const variance = frameTimes.reduce((sum, t) => sum + Math.pow(t - avgFrameTime, 2), 0) / frameTimes.length;
                    const stdDev = Math.sqrt(variance);
                    
                    console.log('\nTest Complete:');
                    console.log(`  Total Frames: ${frameCount}`);
                    console.log(`  Total Time: ${totalTime.toFixed(2)}s`);
                    console.log(`  Average FPS: ${avgFps.toFixed(1)}`);
                    console.log('\nFrame Timing:');
                    console.log(`  Average: ${avgFrameTime.toFixed(2)}ms`);
                    console.log(`  Min: ${minFrameTime}ms`);
                    console.log(`  Max: ${maxFrameTime}ms`);
                    console.log(`  Std Dev: ${stdDev.toFixed(2)}ms`);
                    console.log('  Target (30fps): 33.33ms');
                    
                    camera.stopLiveView();
                    camera.disconnect();
                    cameraBrowser.terminate();
                    return;
                }
            }
            
            setImmediate(captureFrame);
            
        } catch (e) {
            console.error('Frame capture error:', e);
            setImmediate(captureFrame);
        }
    };
    
    setImmediate(captureFrame);
    
    setTimeout(() => {
        if (!shouldStop) {
            shouldStop = true;
            camera.stopLiveView();
            camera.disconnect();
            cameraBrowser.terminate();
        }
    }, 15000);
}

/**
 * Run all tests
 */
if (require.main === module) {
    console.log('LiveView Performance Test Suite');
    console.log('================================\n');
    console.log('This will test different methods for capturing LiveView frames.');
    console.log('Press Ctrl+C to stop at any time.\n');
    
    // Run test 2 (optimized) by default
    // Uncomment others to run different tests
    testOptimizedImmediate();
    
    // testBasicInterval();
    // testWithFrameSaving();
    // testFrameTimingAnalysis();
    
    process.on('SIGINT', () => {
        console.log('\nTest interrupted by user');
        try {
            cameraBrowser.terminate();
        } catch (e) {
            // Ignore
        }
        process.exit(0);
    });
}
