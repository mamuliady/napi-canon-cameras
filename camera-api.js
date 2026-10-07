const AddOn = require('node-gyp-build')(__dirname);

const DATA_URL_PREFIX = 'data:image/jpeg;base64,';

function wrapLiveViewImage(liveViewImage) {
    if (!liveViewImage || typeof liveViewImage !== 'object') {
        return liveViewImage;
    }

    return new Proxy(liveViewImage, {
        get(target, prop, receiver) {
            if (prop === 'getJPEGBuffer' || prop === 'getBlob') {
                return function getJPEGBuffer() {
                    if (typeof target.getJPEGBuffer === 'function') {
                        return target.getJPEGBuffer();
                    }
                    if (typeof target.getBlob === 'function') {
                        return target.getBlob();
                    }

                    const dataUrl = typeof target.getDataURL === 'function' ? target.getDataURL() : '';
                    if (typeof dataUrl !== 'string' || !dataUrl.startsWith(DATA_URL_PREFIX)) {
                        return Buffer.alloc(0);
                    }

                    return Buffer.from(dataUrl.slice(DATA_URL_PREFIX.length), 'base64');
                };
            }

            if (prop === Symbol.for('nodejs.util.inspect.custom')) {
                const customInspect = target[Symbol.for('nodejs.util.inspect.custom')];
                return typeof customInspect === 'function' ? customInspect.bind(target) : undefined;
            }

            const value = Reflect.get(target, prop, target);
            return typeof value === 'function' ? value.bind(target) : value;
        }
    });
}

if (AddOn.Camera && AddOn.Camera.prototype) {
    const originalGetLiveViewImage = AddOn.Camera.prototype.getLiveViewImage;
    if (typeof originalGetLiveViewImage === 'function') {
        AddOn.Camera.prototype.getLiveViewImage = function getLiveViewImageWithJPEGBuffer(...args) {
            return wrapLiveViewImage(originalGetLiveViewImage.apply(this, args));
        };
    }
}

function wrapCamera(camera) {
    if (!camera || typeof camera !== 'object') {
        return camera;
    }
    if (AddOn.Camera && camera instanceof AddOn.Camera) {
        return camera;
    }
    return new Proxy(camera, {
        get(target, prop, receiver) {
            if (prop === 'getLiveViewImage') {
                return function getLiveViewImageWithJPEGBuffer(...args) {
                    return wrapLiveViewImage(target.getLiveViewImage(...args));
                };
            }
            const value = Reflect.get(target, prop, target);
            return typeof value === 'function' ? value.bind(target) : value;
        }
    });
}

AddOn.wrapLiveViewImage = wrapLiveViewImage;
AddOn.wrapCamera = wrapCamera;

AddOn.watchCameras = (timeout) => {
    let running = true;
    const interval = (typeof timeout === 'number' && timeout >= 0) ? timeout : 100;
    const stop = () => {
        running = false;
    };
    const watch = async () => {
        while (running) {
            try {
                if (AddOn.cameraBrowser && typeof AddOn.cameraBrowser.triggerEvents === 'function') {
                    AddOn.cameraBrowser.triggerEvents();
                }
            } catch (err) {
                // Ignore transient trigger errors to prevent crashing the event loop
            }
            await new Promise(resolve => setTimeout(resolve, interval));
        }
    };
    watch().catch((err) => {
        return err;
    });
    return stop;
};

/**
 * @type {CanonApi}
 */
module.exports = AddOn;
