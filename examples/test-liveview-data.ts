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
                const data = image.getDataURL();
                console.log('\nImage data info:');
                console.log('  Type:', typeof data);
                console.log('  Is Buffer:', Buffer.isBuffer(data));
                console.log('  Length:', data.length);
                
                if (typeof data === 'string') {
                    console.log('  First 100 chars:', data.substring(0, 100));
                    console.log('  Starts with data:image:', data.startsWith('data:image'));
                } else if (Buffer.isBuffer(data)) {
                    const buf = data as Buffer;
                    console.log('  First 10 bytes:', buf.slice(0, 10));
                    console.log('  Is JPEG (starts with FFD8):', buf[0] === 0xFF && buf[1] === 0xD8);
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
