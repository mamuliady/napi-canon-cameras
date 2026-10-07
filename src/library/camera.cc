#include <iostream>
#include <napi.h>
#include "api-error.h"
#include "base64.h"
#include "camera-browser.h"
#include "camera-file.h"
#include "camera-property.h"
#include "camera.h"
#include "object-event.h"
#include "state-event.h"
#include "utility.h"
#include "volume.h"
#include "directory.h"
#include "events.h"
#include "live-view-image.h"

namespace CameraApi {

    struct DeviceEventData {
        CameraReference camera;
    };

    struct LiveViewEventData {
        CameraReference camera;
        bool isActive = false;
    };

    struct PropertyEventData {
        CameraReference camera;
        EdsPropertyEvent eventID = 0;
        EdsUInt32 propertyID = 0;
        EdsUInt32 specifier = 0;
    };

    struct ObjectEventData {
        CameraReference camera;
        EdsObjectEvent eventID = 0;
        EdsBaseRef objectRef = nullptr;
    };

    struct StateEventData {
        CameraReference camera;
        EdsStateEvent eventID = 0;
    };

    const LabelMap &CameraCommands() {
        static const LabelMap map = {
            {kEdsCameraCommand_TakePicture, "TakePicture"},
            {kEdsCameraCommand_ExtendShutDownTimer, "ExtendShutDownTimer"},
            {kEdsCameraCommand_BulbStart, "BulbStart"},
            {kEdsCameraCommand_BulbEnd, "BulbEnd"},
            {kEdsCameraCommand_DoEvfAf, "DoEvfAf"},
            {kEdsCameraCommand_DriveLensEvf, "DriveLensEvf"},
            {kEdsCameraCommand_DoClickWBEvf, "DoClickWBEvf"},
            {kEdsCameraCommand_MovieSelectSwON, "MovieSelectSwON"},
            {kEdsCameraCommand_MovieSelectSwOFF, "MovieSelectSwOFF"},
            {kEdsCameraCommand_PressShutterButton, "PressShutterButton"},
            {kEdsCameraCommand_RequestRollPitchLevel, "RequestRollPitchLevel"},
            {kEdsCameraCommand_DrivePowerZoom, "DrivePowerZoom"},
            {kEdsCameraCommand_SetRemoteShootingMode, "SetRemoteShootingMode"},
            {kEdsCameraCommand_RequestSensorCleaning, "RequestSensorCleaning"}

        };
        return map;
    }

    const LabelMap &CameraParametersShutterButton() {
        static const LabelMap map = {
            {kEdsCameraCommand_ShutterButton_OFF, "OFF"},
            {kEdsCameraCommand_ShutterButton_Halfway, "Halfway"},
            {kEdsCameraCommand_ShutterButton_Completely, "Completely"},
            {kEdsCameraCommand_ShutterButton_Halfway_NonAF, "HalfwayNonAF"},
            {kEdsCameraCommand_ShutterButton_Completely_NonAF, "CompletelyNonAF"}
        };
        return map;
    }

    Camera::Camera(const EdsCameraRef edsCamera) {

        if (!edsCamera) {
            throw std::runtime_error("Error - Invalid camera reference.");
        }

        EdsRetain(edsCamera);

        edsCamera_ = edsCamera;
        EdsError error = EDS_ERR_OK;
        error = EdsGetDeviceInfo(edsCamera_, &deviceInfo_);
        if (error != EDS_ERR_OK) {
            throw std::runtime_error("ERROR - failed to get device info");
        }

        error = EdsSetCameraStateEventHandler(
            edsCamera_, kEdsStateEvent_All, Camera::handleStateEvent, this
        );
        if (error != EDS_ERR_OK) {
            throw std::runtime_error("ERROR - failed to set state event handler");
        }
        error = EdsSetPropertyEventHandler(
            edsCamera_, kEdsPropertyEvent_All, Camera::handlePropertyEvent, this
        );
        if (error != EDS_ERR_OK) {
            throw std::runtime_error("ERROR - failed to set property event handler");
        }
        error = EdsSetObjectEventHandler(
            edsCamera_, kEdsObjectEvent_All, Camera::handleObjectEvent, this
        );
        if (error != EDS_ERR_OK) {
            throw std::runtime_error("ERROR - failed to set object event handler");
        }
    }

    Camera::~Camera() {
        if (edsCamera_) {
            EdsSetCameraStateEventHandler(edsCamera_, kEdsStateEvent_All, nullptr, this);
            EdsSetPropertyEventHandler(edsCamera_, kEdsPropertyEvent_All, nullptr, this);
            EdsSetObjectEventHandler(edsCamera_, kEdsObjectEvent_All, nullptr, this);
            if (isConnected_) {
                stopLiveView();
                isConnected_ = false;
                EdsCloseSession(edsCamera_);
            }
            EdsRelease(edsCamera_);
            edsCamera_ = nullptr;
        }
        if (tsEmit_) {
            tsEmit_.Release();
            tsEmit_ = nullptr;
        }
    }

    Napi::ThreadSafeFunction &Camera::getEventEmit() {
        return tsEmit_;
    }

    bool Camera::hasEventEmit() {
        return tsEmit_;
    }

    void Camera::attachEventEmit(const Napi::Function &emit) {
        if (tsEmit_) {
            tsEmit_.Release();
            tsEmit_ = nullptr;
        }
        tsEmit_ = Napi::ThreadSafeFunction::New(
            emit.Env(),
            emit,  // JavaScript function called asynchronously
            "EDSDK Event",         // Name
            0,                       // Unlimited queue
            1,                       // Only one thread will use this initially
            [](Napi::Env) {        // Finalizer used to clean threads up
                //nativeThread.join();
            }
        );
    }

    void Camera::detachEventEmit() {
        if (tsEmit_) {
            tsEmit_.Release();
            tsEmit_ = nullptr;
        }
    }

    CameraReference Camera::create(const EdsCameraRef &edsCamera) {
        return CameraReference(new Camera(edsCamera))->shared_from_this();
    }

    EdsError Camera::connect(bool shouldKeepAlive = false) {
        shouldKeepAlive_ = shouldKeepAlive;
        if (isConnected_) {
            return EDS_ERR_OK;
        }

        EdsUInt32 id;
        id = kEdsPropID_UTCTime;
        EdsSetPropertyData(edsCamera_, 0x01000000, 0x51DD2696, sizeof(id), &id);
        id = kEdsPropID_TimeZone;
        EdsSetPropertyData(edsCamera_, 0x01000000, 0xFA71F7, sizeof(id), &id);
        id = kEdsPropID_SummerTimeSetting;
        EdsSetPropertyData(edsCamera_, 0x01000000, 0x9780670, sizeof(id), &id);
        id = kEdsPropID_Aspect;
        EdsSetPropertyData(edsCamera_, 0x01000000, 0x3FB1718B, sizeof(id), &id);
        // difotoin: the body's overheat indicator. Without this unlock every read
        // of kEdsPropID_TempStatus fails with EDS_ERR_PROTECTION_VIOLATION (0x09)
        // — hardware-confirmed on an EOS RP, 2026-07-28. The gate property
        // (0x01000000) and the per-property key are Canon's own, taken from the
        // EDSDK 13.18.0 sample MultiCamCui/src/CameraModel.cpp:63. Like every
        // unlock above it MUST stay before EdsOpenSession; after the session is
        // open the camera refuses it.
        id = kEdsPropID_TempStatus;
        EdsSetPropertyData(edsCamera_, 0x01000000, 0x14840DF1, sizeof(id), &id);

        EdsError error = EdsOpenSession(edsCamera_);
        if (error != EDS_ERR_OK) {
            return error;
        }
        isConnected_ = true;
        emitCameraEvent(EventName_CameraConnect);
        return EDS_ERR_OK;
    }

    EdsError Camera::disconnect() {
        if (!isConnected_) {
            return EDS_ERR_SESSION_NOT_OPEN;
        }
        stopLiveView();
        isConnected_ = false;
        EdsError error = EdsCloseSession(edsCamera_);
        if (error == EDS_ERR_OK) {
            emitCameraEvent(EventName_CameraDisconnect);
        }
        return error;
    }

    void Camera::emitCameraEvent(const std::string &eventName) {
        if (!this->hasEventEmit() && !CameraBrowser::instance()->hasEventEmit()) {
            return;
        }

        auto jsCallback = [eventName](
            Napi::Env env, Napi::Function jsCallback, DeviceEventData *dataPtr
        ) {
            Napi::Object event = Napi::Object::New(env);
            event.Set("camera", CameraWrap::NewInstance(env, dataPtr->camera));
            jsCallback.Call(
                {
                    Napi::String::New(env, eventName),
                    event
                }
            );
            delete dataPtr;
        };

        if (this->hasEventEmit()) {
            DeviceEventData *dataPtr = new DeviceEventData{this->shared_from_this()};
            if (this->getEventEmit().NonBlockingCall(dataPtr, jsCallback) != napi_ok) {
                delete dataPtr;
            }
        }
        if (CameraBrowser::instance()->hasEventEmit()) {
            DeviceEventData *dataPtr = new DeviceEventData{this->shared_from_this()};
            if (CameraBrowser::instance()->getEventEmit().NonBlockingCall(dataPtr, jsCallback) != napi_ok) {
                delete dataPtr;
            }
        }
    }

    EdsError Camera::sendCommand(EdsCameraCommand command, EdsInt32 parameter) {
        if (!isConnected_) {
            return EDS_ERR_SESSION_NOT_OPEN;
        }
        return EdsSendCommand(edsCamera_, command, parameter);
    }

    EdsError Camera::takePicture() {
        if (!isConnected_) {
            return EDS_ERR_SESSION_NOT_OPEN;
        }
        EdsError error = EDS_ERR_OK;
        if (!isLegacy_) {
            error = EdsSendCommand(
                edsCamera_, kEdsCameraCommand_PressShutterButton,
                kEdsCameraCommand_ShutterButton_Halfway
            );
            if (EDS_ERR_OK == error) {
                error = EdsSendCommand(
                    edsCamera_, kEdsCameraCommand_PressShutterButton,
                    kEdsCameraCommand_ShutterButton_Completely
                );
            }
            if (EDS_ERR_OK == error) {
                error = EdsSendCommand(
                    edsCamera_, kEdsCameraCommand_PressShutterButton,
                    kEdsCameraCommand_ShutterButton_OFF
                );
            }
        }
        if (isLegacy_ || EDS_ERR_INVALID_PARAMETER == error) {
            isLegacy_ = true;
            EdsSendStatusCommand(edsCamera_, kEdsCameraStatusCommand_UILock, 0);
            error = EdsSendCommand(edsCamera_, kEdsCameraCommand_TakePicture, 0);
            EdsSendStatusCommand(edsCamera_, kEdsCameraStatusCommand_UIUnLock, 0);
        }
        return error;
    }

    EdsError Camera::startLiveView() {
        if (!isConnected_) {
            return EDS_ERR_SESSION_NOT_OPEN;
        }
        EdsError error;
        EdsUInt32 device;
        error = EdsGetPropertyData(
            edsCamera_, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device
        );
        if (error == EDS_ERR_OK) {
            device |= kEdsEvfOutputDevice_PC;
            error = EdsSetPropertyData(
                edsCamera_, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device
            );
            if (error == EDS_ERR_OK) {
                updateLiveViewStatus();
            }
        }
        return error;
    }

    EdsError Camera::stopLiveView() {
        if (!isConnected_) {
            return EDS_ERR_SESSION_NOT_OPEN;
        }
        EdsError error = EDS_ERR_OK;
        EdsUInt32 device;
        error = EdsGetPropertyData(
            edsCamera_, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device
        );
        if (error == EDS_ERR_OK) {
            device &= ~kEdsEvfOutputDevice_PC;
            error = EdsSetPropertyData(
                edsCamera_, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device
            );
            if (error == EDS_ERR_OK) {
                updateLiveViewStatus();
            }
        }
        return error;
    }

    bool Camera::updateLiveViewStatus() {
        if (!edsCamera_ || !isConnected_) {
            return hasActiveLiveView_;
        }
        EdsUInt32 device = 0;
        EdsGetPropertyData(edsCamera_, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device);
        bool activeLiveView = ((device & kEdsEvfOutputDevice_PC) == kEdsEvfOutputDevice_PC);
        if (hasActiveLiveView_ != activeLiveView) {
            hasActiveLiveView_ = activeLiveView;

            if (!hasEventEmit() && !CameraBrowser::instance()->hasEventEmit()) {
                return hasActiveLiveView_;
            }

            auto jsCallback = [](
                Napi::Env env, Napi::Function jsCallback, LiveViewEventData *dataPtr
            ) {
                Napi::Object event = Napi::Object::New(env);
                event.Set("camera", CameraWrap::NewInstance(env, dataPtr->camera));
                jsCallback.Call(
                    {
                        Napi::String::New(
                            env,
                            dataPtr->isActive ? EventName_LiveViewStart : EventName_LiveViewStop
                        ),
                        event
                    }
                );
                delete dataPtr;
            };

            if (hasEventEmit()) {
                LiveViewEventData *dataPtr = new LiveViewEventData{this->shared_from_this(), activeLiveView};
                if (getEventEmit().NonBlockingCall(dataPtr, jsCallback) != napi_ok) {
                    delete dataPtr;
                }
            }
            if (CameraBrowser::instance()->hasEventEmit()) {
                LiveViewEventData *dataPtr = new LiveViewEventData{this->shared_from_this(), activeLiveView};
                if (CameraBrowser::instance()->getEventEmit().NonBlockingCall(dataPtr, jsCallback) != napi_ok) {
                    delete dataPtr;
                }
            }
        }
        return hasActiveLiveView_;
    }

    EdsError Camera::downloadLiveViewImage(std::string &image) {
        if (!hasActiveLiveView_) {
            return EDS_ERR_OK;
        }
        EdsError error = EDS_ERR_OK;
        EdsStreamRef stream = nullptr;
        EdsEvfImageRef evfImage = nullptr;

        error = EdsCreateMemoryStream(0, &stream);
        if (error == EDS_ERR_OK) {
            error = EdsCreateEvfImageRef(stream, &evfImage);
        }
        if (error == EDS_ERR_OK) {
            error = EdsDownloadEvfImage(edsCamera_, evfImage);
        }
        if (error == EDS_ERR_OK) {
            EdsUInt64 imageDataLength;
            int imageStringLength;
            unsigned char *imageData;

            EdsGetLength(stream, &imageDataLength);
            if (imageDataLength > 0) {
                EdsGetPointer(stream, (EdsVoid **) &imageData);

                char *imageString = base64(imageData, (int) imageDataLength, &imageStringLength);
                image.assign(imageString);
                free(imageString);
            }
        }
        if (stream != nullptr) {
            EdsRelease(stream);
            stream = nullptr;
        }
        if (evfImage != nullptr) {
            EdsRelease(evfImage);
            evfImage = nullptr;
        }
        return error;
    }

    EdsError __stdcall Camera::handleStateEvent(
        EdsStateEvent inEvent, EdsUInt32 inEventData, EdsVoid *inContext
    ) {
        auto *c = (Camera *) inContext;
        if (!c) {
            return EDS_ERR_OK;
        }
        CameraReference camera;
        try {
            camera = c->shared_from_this();
        } catch (...) {
            return EDS_ERR_OK;
        }

        switch (inEvent) {
            case kEdsStateEvent_Shutdown:
                camera->disconnect();
                CameraBrowser::instance()->removeCamera(camera);
                break;
            case kEdsStateEvent_WillSoonShutDown:
                if (camera->isConnected_ && camera->shouldKeepAlive_) {
                    EdsSendCommand(camera->edsCamera_, kEdsCameraCommand_ExtendShutDownTimer, 0);
                    camera->emitCameraEvent(EventName_KeepAlive);
                    break;
                }
            default:
                if (!camera->hasEventEmit() && !CameraBrowser::instance()->hasEventEmit()) {
                    break;
                }

                auto jsCallback = [](
                    Napi::Env env, Napi::Function jsCallback, StateEventData *dataPtr
                ) {
                    Napi::Object event = Napi::Object::New(env);
                    event.Set("camera", CameraWrap::NewInstance(env, dataPtr->camera));
                    event.Set("stateEvent", StateEvent::NewInstance(env, dataPtr->eventID));
                    jsCallback.Call(
                        {
                            Napi::String::New(env, EventName_StateChange),
                            event
                        }
                    );
                    delete dataPtr;
                };

                if (camera->hasEventEmit()) {
                    StateEventData *dataPtr = new StateEventData{camera, inEvent};
                    if (camera->getEventEmit().NonBlockingCall(dataPtr, jsCallback) != napi_ok) {
                        delete dataPtr;
                    }
                }
                if (CameraBrowser::instance()->hasEventEmit()) {
                    StateEventData *dataPtr = new StateEventData{camera, inEvent};
                    if (CameraBrowser::instance()->getEventEmit().NonBlockingCall(dataPtr, jsCallback) != napi_ok) {
                        delete dataPtr;
                    }
                }
                break;
        }
        return EDS_ERR_OK;
    }

    EdsError __stdcall Camera::handlePropertyEvent(
        EdsPropertyEvent inEvent, EdsUInt32 inPropertyID, EdsUInt32 inParam, EdsVoid *inContext
    ) {
        auto *c = (Camera *) inContext;
        if (!c) {
            return EDS_ERR_OK;
        }
        CameraReference camera;
        try {
            camera = c->shared_from_this();
        } catch (...) {
            return EDS_ERR_OK;
        }

        switch (inPropertyID) {
            case kEdsPropID_Evf_OutputDevice:
                camera->updateLiveViewStatus();
                break;
        }

        if (!camera->hasEventEmit() && !CameraBrowser::instance()->hasEventEmit()) {
            return EDS_ERR_OK;
        }

        auto jsCallback = [](Napi::Env env, Napi::Function jsCallback, PropertyEventData *dataPtr) {
            Napi::Object event = Napi::Object::New(env);
            event.Set("camera", CameraWrap::NewInstance(env, dataPtr->camera));
            event.Set(
                "property",
                CameraProperty::NewInstance(
                    env,
                    dataPtr->camera,
                    dataPtr->propertyID,
                    dataPtr->specifier
                )
            );
            jsCallback.Call(
                {
                    Napi::String::New(
                        env,
                        dataPtr->eventID == kEdsPropertyEvent_PropertyDescChanged
                        ? EventName_PropertyChangeOptions
                        : EventName_PropertyChangeValue
                    ),
                    event
                }
            );
            delete dataPtr;
        };

        if (camera->hasEventEmit()) {
            PropertyEventData *dataPtr = new PropertyEventData{camera, inEvent, inPropertyID, inParam};
            if (camera->getEventEmit().NonBlockingCall(dataPtr, jsCallback) != napi_ok) {
                delete dataPtr;
            }
        }
        if (CameraBrowser::instance()->hasEventEmit()) {
            PropertyEventData *dataPtr = new PropertyEventData{camera, inEvent, inPropertyID, inParam};
            if (CameraBrowser::instance()->getEventEmit().NonBlockingCall(dataPtr, jsCallback) != napi_ok) {
                delete dataPtr;
            }
        }

        return EDS_ERR_OK;
    }

    EdsError __stdcall Camera::handleObjectEvent(
        EdsObjectEvent inEvent, EdsBaseRef inRef, EdsVoid *inContext
    ) {
        auto *c = (Camera *) inContext;
        if (!c) {
            return EDS_ERR_OK;
        }
        CameraReference camera;
        try {
            camera = c->shared_from_this();
        } catch (...) {
            return EDS_ERR_OK;
        }

        if (!camera->hasEventEmit() && !CameraBrowser::instance()->hasEventEmit()) {
            return EDS_ERR_OK;
        }

        auto jsCallback = [](
            Napi::Env env, Napi::Function jsCallback, ObjectEventData *dataPtr
        ) {
            EdsError error = EDS_ERR_OK;
            Napi::Object event = Napi::Object::New(env);
            event.Set("camera", CameraWrap::NewInstance(env, dataPtr->camera));
            switch (dataPtr->eventID) {
                case kEdsObjectEvent_DirItemRequestTransferDT:
                case kEdsObjectEvent_DirItemRequestTransfer:
                    event.Set(
                        "file",
                        CameraFile::NewInstance(env, (EdsDirectoryItemRef) dataPtr->objectRef)
                    );
                    jsCallback.Call(
                        {
                            Napi::String::New(env, EventName_DownloadRequest),
                            event
                        }
                    );
                    break;
                case kEdsObjectEvent_DirItemCreated:
                    EdsDirectoryItemInfo entryInfo;
                    error = EdsGetDirectoryItemInfo(dataPtr->objectRef, &entryInfo);
                    if (error == EDS_ERR_OK) {
                        if (entryInfo.isFolder) {
                            event.Set(
                                "directory", Directory::NewInstance(env, dataPtr->objectRef)
                            );
                            jsCallback.Call(
                                {
                                    Napi::String::New(env, EventName_DirectoryCreate),
                                    event
                                }
                            );
                        } else {
                            event.Set(
                                "file", CameraFile::NewInstance(env, dataPtr->objectRef)
                            );
                            jsCallback.Call(
                                {
                                    Napi::String::New(env, EventName_FileCreate),
                                    event
                                }
                            );
                        }
                    }
                    break;

                default:
                    event.Set("objectEvent", ObjectEvent::NewInstance(env, dataPtr->eventID));
                    jsCallback.Call(
                        {
                            Napi::String::New(env, EventName_ObjectChange),
                            event
                        }
                    );
                    break;
            }
            if (dataPtr->objectRef) {
                EdsRelease(dataPtr->objectRef);
                dataPtr->objectRef = nullptr;
            }
            delete dataPtr;
        };

        if (camera->hasEventEmit()) {
            if (inRef) {
                EdsRetain(inRef);
            }
            ObjectEventData *dataPtr = new ObjectEventData{camera, inEvent, inRef};
            if (camera->getEventEmit().NonBlockingCall(dataPtr, jsCallback) != napi_ok) {
                if (inRef) {
                    EdsRelease(inRef);
                }
                delete dataPtr;
            }
        }
        if (CameraBrowser::instance()->hasEventEmit()) {
            if (inRef) {
                EdsRetain(inRef);
            }
            ObjectEventData *dataPtr = new ObjectEventData{camera, inEvent, inRef};
            if (CameraBrowser::instance()->getEventEmit().NonBlockingCall(dataPtr, jsCallback) != napi_ok) {
                if (inRef) {
                    EdsRelease(inRef);
                }
                delete dataPtr;
            }
        }

        return EDS_ERR_OK;
    }

    /* JS Wrapper Class */

    CameraWrap::CameraWrap(const Napi::CallbackInfo &info)
        : Napi::ObjectWrap<CameraWrap>(info) {

        if (info.Length() > 0) {
            if (info[0].IsExternal()) {
                auto external = info[0].As<Napi::External<CameraReference>>();
                camera_ = *external.Data();
            } else if (info[0].IsNumber()) {
                camera_ = CameraBrowser::instance()->getCameraAtIndex(
                    info[0].As<Napi::Number>().Int32Value());
            } else if (info[0].IsString()) {
                camera_ = CameraBrowser::instance()->getCameraAtPort(
                    info[0].As<Napi::String>().Utf8Value());
            }
        } else {
            camera_ = CameraBrowser::instance()->getCameraAtIndex(0);
        }
        if (!camera_) {
            ApiError::Throw(info.Env(), EDS_ERR_DEVICE_NOT_FOUND);
        }
    }

    CameraWrap::~CameraWrap() {
        camera_ = nullptr;
    }

    Napi::Object CameraWrap::NewInstance(Napi::Env env, CameraReference camera) {
        Napi::EscapableHandleScope scope(env);
        Napi::Object wrap = JSConstructor().New(
            {
                Napi::External<CameraReference>::New(env, &camera)
            }
        );
        return scope.Escape(napi_value(wrap)).ToObject();
    }

    Napi::Object CameraWrap::NewInstance(Napi::Env env, EdsCameraRef edsCameraRef) {
        CameraReference camera = CameraBrowser::instance()->getCameraByEdsReference(edsCameraRef);
        Napi::EscapableHandleScope scope(env);
        Napi::Object wrap = JSConstructor().New(
            {
                Napi::External<CameraReference>::New(env, &camera)
            }
        );
        return scope.Escape(napi_value(wrap)).ToObject();
    }

    Napi::Value CameraWrap::GetDescription(const Napi::CallbackInfo &info) {
        return Napi::String::New(info.Env(), camera_->getDescription());
    }

    Napi::Value CameraWrap::GetPortName(const Napi::CallbackInfo &info) {
        return Napi::String::New(info.Env(), camera_->getPortName());
    }

    Napi::Value CameraWrap::ToStringTag(const Napi::CallbackInfo &info) {
        return Napi::String::New(info.Env(), CameraWrap::JSClassName);
    }

    Napi::Value CameraWrap::Inspect(const Napi::CallbackInfo &info) {
        auto env = info.Env();
        auto stylize = info[1].As<Napi::Object>().Get("stylize").As<Napi::Function>();
        std::string output = stylize.Call(
            {Napi::String::New(env, CameraWrap::JSClassName), Napi::String::New(env, "special")}
        ).As<Napi::String>().Utf8Value();
        output.append(" <");
        output.append(
            stylize.Call(
                {GetDescription(info), Napi::String::New(env, "string")}
            ).As<Napi::String>().Utf8Value()
        );
        output.append(">");
        return Napi::String::New(env, output);
    }

    Napi::Value CameraWrap::Connect(const Napi::CallbackInfo &info) {
        bool keepAlive = false;
        if (info.Length() > 0 && info[0].IsBoolean()) {
            keepAlive = info[0].As<Napi::Boolean>().Value();
        }
        return ApiError::ThrowIfFailed(info.Env(), camera_->connect(keepAlive));
    }

    Napi::Value CameraWrap::Disconnect(const Napi::CallbackInfo &info) {
        return ApiError::ThrowIfFailed(info.Env(), camera_->disconnect());
    }

    Napi::Value CameraWrap::SetEventHandler(const Napi::CallbackInfo &info) {
        if (info.Length() > 0 && info[0].IsFunction()) {
            camera_->attachEventEmit(info[0].As<Napi::Function>());
        } else if (info.Length() > 0 && (info[0].IsNull() || info[0].IsUndefined())) {
            camera_->detachEventEmit();
        }
        return info.Env().Undefined();
    }

    Napi::Value CameraWrap::GetProperty(const Napi::CallbackInfo &info) {
        if (info.Length() < 1) {
            throw Napi::TypeError::New(
                info.Env(), "Argument 0 must be an property identifier."
            );
        }
        return CameraProperty::NewInstance(
            info.Env(),
            camera_,
            CameraProperty::GetIDFor(info[0]),
            (info.Length() > 1) ? info[1].As<Napi::Number>().Int32Value() : 0
        );
    }

    Napi::Value CameraWrap::SetProperty(const Napi::CallbackInfo &info) {
        if (info.Length() < 1) {
            throw Napi::TypeError::New(
                info.Env(), "Argument 0 must be an property identifier."
            );
        }
        if (info.Length() < 2) {
            throw Napi::TypeError::New(
                info.Env(), "Argument 1 must be an property value."
            );
        }
        uint32_t specifier = 0;
        Napi::Value val = info[1];
        if (info.Length() > 2) {
            specifier = info[1].As<Napi::Number>().Uint32Value();
            val = info[2];
        }
        auto property = Napi::ObjectWrap<CameraProperty>::Unwrap(
            CameraProperty::NewInstance(
                info.Env(),
                camera_,
                CameraProperty::GetIDFor(info[0]),
                specifier
            )
        );
        property->SetValue(info, val);
        return info.Env().Undefined();
    }

    Napi::Value CameraWrap::SetProperties(const Napi::CallbackInfo &info) {
        if (info.Length() > 0 && info[0].IsObject()) {
            auto properties = info[0].As<Napi::Object>();
            auto propertyNames = properties.GetPropertyNames();
            for (uint32_t i = 0; i < propertyNames.Length(); i++) {
                auto propertyLabel = propertyNames.Get(i).As<Napi::String>().Utf8Value();
                auto propertyID = CameraProperty::GetIDFor(propertyNames.Get(i));
                if (!propertyID) {
                    ApiError::Throw(info.Env(), EDS_ERR_PROPERTIES_MISMATCH);
                }
                auto property = Napi::ObjectWrap<CameraProperty>::Unwrap(
                    CameraProperty::NewInstance(
                        info.Env(), GetCameraReference(), propertyID, 0
                    )
                );
                property->SetValue(info, properties.Get(propertyLabel));
            }
        }
        return Napi::Value();
    }

    Napi::Value CameraWrap::SendCommand(const Napi::CallbackInfo &info) {
        if (info.Length() < 1 || !info[0].IsNumber()) {
            throw Napi::TypeError::New(
                info.Env(), "Argument 0 must be an command identifier."
            );
        }
        return ApiError::ThrowIfFailed(
            info.Env(),
            camera_->sendCommand(
                info[0].As<Napi::Number>().Int32Value(),
                (info.Length() > 1) ? info[1].As<Napi::Number>().Int32Value() : 0
            )
        );
    }

    Napi::Value CameraWrap::TakePicture(const Napi::CallbackInfo &info) {
        return ApiError::ThrowIfFailed(info.Env(), camera_->takePicture());
    }

    Napi::Value CameraWrap::StartLiveView(const Napi::CallbackInfo &info) {
        return ApiError::ThrowIfFailed(info.Env(), camera_->startLiveView());
    }

    Napi::Value CameraWrap::StopLiveView(const Napi::CallbackInfo &info) {
        return ApiError::ThrowIfFailed(info.Env(), camera_->stopLiveView());
    }

    Napi::Value CameraWrap::IsLiveViewActive(const Napi::CallbackInfo &info) {
        return Napi::Boolean::New(info.Env(), camera_->isLiveViewActive());
    }

    Napi::Value CameraWrap::DownloadLiveViewImage(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();
        std::string image;
        EdsError error = camera_->downloadLiveViewImage(image);
        return ApiError::ThrowIfFailed(env, error, Napi::String::New(env, image));
    }

    Napi::Value CameraWrap::GetLiveViewImage(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();
        if (!camera_ || !camera_->isConnected()) {
            return env.Undefined();
        }
        try {
            return LiveViewImage::NewInstance(env, camera_->getEdsReference());
        } catch (...) {
            return env.Undefined();
        }
    }

    Napi::Value CameraWrap::GetVolumes(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        EdsUInt32 volumeCount = 0;
        auto cameraRef = camera_->getEdsReference();
        auto error = EdsGetChildCount(cameraRef, &volumeCount);
        ApiError::ThrowIfFailed(env, error);

        auto volumes = Napi::Array::New(env);
        for (uint32_t idx = 0; idx < volumeCount; idx++) {
            EdsVolumeRef edsVolume = nullptr;
            error = EdsGetChildAtIndex(cameraRef, idx, &edsVolume);
            ApiError::ThrowIfFailed(info.Env(), error);
            volumes.Set(idx, Volume::NewInstance(env, edsVolume));
            EdsRelease(edsVolume);
        }
        return volumes;
    }

    void CameraWrap::Init(Napi::Env env, Napi::Object exports) {
        Napi::HandleScope scope(env);

        Napi::Object Commands = Napi::Object::New(env);
        for (const auto &it: CameraCommands()) {
            Commands.Set(
                Napi::String::New(env, it.second), Napi::Number::New(env, it.first)
            );
        }
        Napi::Object ShutterButtonParameters = Napi::Object::New(env);
        for (const auto &it: CameraParametersShutterButton()) {
            ShutterButtonParameters.Set(
                Napi::String::New(env, it.second), Napi::Number::New(env, it.first)
            );
        }

        Napi::Object eventNames = Napi::Object::New(env);
        for (auto it: CameraEvents) {
            eventNames.Set(it, it);
        }

        Napi::Function func = DefineClass(
            env,
            CameraWrap::JSClassName,
            {
                InstanceAccessor<&CameraWrap::GetDescription>("description"),
                InstanceAccessor<&CameraWrap::GetPortName>("portName"),

                InstanceAccessor<&CameraWrap::ToStringTag>(
                    Napi::Symbol::WellKnown(env, "toStringTag")),
                InstanceMethod(
                    GetPublicSymbol(env, "nodejs.util.inspect.custom"), &CameraWrap::Inspect
                ),

                InstanceMethod("connect", &CameraWrap::Connect),
                InstanceMethod("disconnect", &CameraWrap::Disconnect),
                InstanceMethod("setEventHandler", &CameraWrap::SetEventHandler),
                InstanceMethod("getProperty", &CameraWrap::GetProperty),
                InstanceMethod("setProperty", &CameraWrap::SetProperty),
                InstanceMethod("setProperties", &CameraWrap::SetProperties),
                InstanceMethod("sendCommand", &CameraWrap::SendCommand),
                InstanceMethod("takePicture", &CameraWrap::TakePicture),
                InstanceMethod("startLiveView", &CameraWrap::StartLiveView),
                InstanceMethod("isLiveViewActive", &CameraWrap::IsLiveViewActive),
                InstanceMethod("stopLiveView", &CameraWrap::StopLiveView),
                InstanceMethod("downloadLiveViewImage", &CameraWrap::DownloadLiveViewImage),
                InstanceMethod("getLiveViewImage", &CameraWrap::GetLiveViewImage),
                InstanceMethod("getVolumes", &CameraWrap::GetVolumes),

                StaticValue("EventName", eventNames, napi_enumerable),
                StaticValue("Command", Commands, napi_enumerable),
                StaticValue("PressShutterButton", ShutterButtonParameters, napi_enumerable)
            }
        );
        JSConstructor(&func);
        exports.Set(CameraWrap::JSClassName, func);
    }
}
