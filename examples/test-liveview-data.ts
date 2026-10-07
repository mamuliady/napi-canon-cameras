import { cameraBrowser, CameraProperty } from '../';

try {
    const camera = cameraBrowser.getCamera();
    if (!camera) {
        console.error('No camera found!');
        process.exit(1);
    }
    
    console.log('Camera:', camera.description);
    camera.connect();
    
    if (!camera.getProperty(CameraProperty.ID.Evf_Mode).available) {
        console.error('LiveView not available');
        process.exit(1);
    }
    
    camera.startLiveView();
    console.log('LiveView started, waiting for frames...');
    
    setTimeout(() => {
        try {
            const image = camera.getLiveViewImage();
            if (image) {
                const dataUrl = image.getDataURL();
                const blob = image.getBlob();
                console.log('\nImage dataURL info:');
                console.log('  Type:', typeof dataUrl);
                console.log('  Length:', dataUrl ? dataUrl.length : 0);

                console.log('\nImage getBlob() info:');
                console.log('  Is Buffer/Uint8Array:', Buffer.isBuffer(blob) || blob instanceof Uint8Array);
                console.log('  Byte length:', blob ? blob.length : 0);
                if (blob && blob.length > 2) {
                    console.log('  First 10 bytes:', Buffer.from(blob).slice(0, 10));
                    console.log('  Is JPEG (starts with FFD8):', blob[0] === 0xFF && blob[1] === 0xD8);
                }
                
                const coords = image.coordinateSystem;
                console.log('\nImage dimensions:', coords.width, 'x', coords.height);
            } else {
                console.log('No image returned');
            }
        } catch (e) {
            console.error('Error:', e);
        }
        
        camera.stopLiveView();
        camera.disconnect();
        process.exit(0);
    }, 2000);
    
} catch (e) {
    console.error('Error:', e);
    process.exit(1);
}
