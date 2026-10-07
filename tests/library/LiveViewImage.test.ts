/* eslint-disable @typescript-eslint/no-explicit-any */
import * as CameraApi from '../../camera-api';

describe('LiveViewImage getBlob and getJPEGBuffer', () => {
    test('camera-api exposes cameraBrowser with getCamera and getCameras', () => {
        expect(CameraApi.cameraBrowser).toBeDefined();
        expect(typeof CameraApi.cameraBrowser.getCamera).toBe('function');
        expect(typeof CameraApi.cameraBrowser.getCameras).toBe('function');
    });

    test('proxy decodes base64 data URL to buffer for getJPEGBuffer and getBlob', () => {
        const testJpegBytes = Buffer.from([0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46]);
        const testDataUrl = `data:image/jpeg;base64,${testJpegBytes.toString('base64')}`;

        const mockCamera = {
            getLiveViewImage() {
                return {
                    getDataURL() {
                        return testDataUrl;
                    },
                    coordinateSystem: { width: 1024, height: 680 }
                };
            }
        };

        const wrappedCamera = (CameraApi as any).wrapCamera(mockCamera);
        const liveViewImage = wrappedCamera.getLiveViewImage();

        expect(typeof liveViewImage.getJPEGBuffer).toBe('function');
        expect(typeof liveViewImage.getBlob).toBe('function');

        const jpegBuffer = liveViewImage.getJPEGBuffer();
        expect(Buffer.isBuffer(jpegBuffer) || jpegBuffer instanceof Uint8Array).toBe(true);
        expect(Buffer.from(jpegBuffer)).toEqual(testJpegBytes);

        const blobBuffer = liveViewImage.getBlob();
        expect(Buffer.isBuffer(blobBuffer) || blobBuffer instanceof Uint8Array).toBe(true);
        expect(Buffer.from(blobBuffer)).toEqual(testJpegBytes);
    });

    test('returns empty buffer when getDataURL is invalid or missing', () => {
        const mockCamera = {
            getLiveViewImage() {
                return {
                    getDataURL() {
                        return '';
                    }
                };
            }
        };

        const wrappedCamera = (CameraApi as any).wrapCamera(mockCamera);
        const liveViewImage = wrappedCamera.getLiveViewImage();

        const jpegBuffer = liveViewImage.getJPEGBuffer();
        expect(jpegBuffer.length).toBe(0);

        const blobBuffer = liveViewImage.getBlob();
        expect(blobBuffer.length).toBe(0);
    });

    test('calls native getJPEGBuffer and getBlob directly when present', () => {
        const nativeBuffer = Buffer.from([0x01, 0x02, 0x03]);
        const mockCamera = {
            getLiveViewImage() {
                return {
                    getJPEGBuffer() {
                        return nativeBuffer;
                    },
                    getBlob() {
                        return nativeBuffer;
                    }
                };
            }
        };

        const wrappedCamera = (CameraApi as any).wrapCamera(mockCamera);
        const liveViewImage = wrappedCamera.getLiveViewImage();

        expect(liveViewImage.getJPEGBuffer()).toBe(nativeBuffer);
        expect(liveViewImage.getBlob()).toBe(nativeBuffer);
    });
});
