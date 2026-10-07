#include "live-view-image.h"
#include "base64.h"
#include "api-error.h"
#include "option.h"
#include "utility.h"

namespace CameraApi {

    LiveViewImage::LiveViewImage(const Napi::CallbackInfo &info) : Napi::ObjectWrap<LiveViewImage>(info) {
        Napi::Env env = info.Env();
        Napi::HandleScope scope(env);


        if (!(info.Length() > 0 && info[0].IsExternal())) {
            throw Napi::TypeError::New(
                info.Env(), "Argument 0 must be a camera reference."
            );
        }

        auto external = info[0].As<Napi::External<EdsCameraRef >>();
        EdsCameraRef edsCameraRef = *external.Data();

        EdsError error = fetch(edsCameraRef);
        ApiError::ThrowIfFailed(env, error);

    }

    EdsError LiveViewImage::fetch(EdsCameraRef edsCameraRef) {
        EdsError error = EDS_ERR_OK;

        error = EdsCreateMemoryStream(0, &streamRef_);
        if (error == EDS_ERR_OK) {
            error = EdsCreateEvfImageRef(streamRef_, &imageRef_);
        }
        if (error == EDS_ERR_OK) {
            error = EdsDownloadEvfImage(edsCameraRef, imageRef_);
        }
        if (error != EDS_ERR_OK) {
            if (imageRef_ != nullptr) {
                EdsRelease(imageRef_);
                imageRef_ = nullptr;
            }
            if (streamRef_ != nullptr) {
                EdsRelease(streamRef_);
                streamRef_ = nullptr;
            }
        }
        return error;
    }

    LiveViewImage::~LiveViewImage() {
        if (streamRef_ != nullptr) {
            EdsRelease(streamRef_);
            streamRef_ = nullptr;
        }
        if (imageRef_ != nullptr) {
            EdsRelease(imageRef_);
            imageRef_ = nullptr;
        }
    }

    Napi::Value LiveViewImage::GetDataURL(const Napi::CallbackInfo &info) {
        if (streamRef_ == nullptr) {
            return info.Env().Undefined();
        }
        EdsUInt64 imageDataLength = 0;
        int imageStringLength = 0;
        unsigned char *imageData = nullptr;

        EdsGetLength(streamRef_, &imageDataLength);
        if (imageDataLength > 0) {
            EdsGetPointer(streamRef_, (EdsVoid **) &imageData);
            if (imageData != nullptr) {
                char *imageString = base64(imageData, (int) imageDataLength, &imageStringLength);
                if (imageString != nullptr) {
                    std::string encodedData = "data:image/jpeg;base64,";
                    encodedData.append(imageString, imageStringLength);
                    free(imageString);
                    return Napi::String::New(info.Env(), encodedData);
                }
            }
        }
        return info.Env().Undefined();
    }

    Napi::Value LiveViewImage::GetJPEGBuffer(const Napi::CallbackInfo &info) {
        if (streamRef_ == nullptr) {
            return Napi::Buffer<unsigned char>::New(info.Env(), 0);
        }
        EdsUInt64 imageDataLength = 0;
        unsigned char *imageData = nullptr;

        EdsGetLength(streamRef_, &imageDataLength);
        if (imageDataLength > 0) {
            EdsGetPointer(streamRef_, (EdsVoid **) &imageData);
            if (imageData != nullptr) {
                return Napi::Buffer<unsigned char>::Copy(
                    info.Env(),
                    imageData,
                    static_cast<size_t>(imageDataLength)
                );
            }
        }

        return Napi::Buffer<unsigned char>::New(info.Env(), 0);
    }

    Napi::Value LiveViewImage::GetCoordinateSystem(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();
        if (imageRef_ == nullptr) {
            return env.Undefined();
        }
        EdsSize coordinateSystem;
        EdsError error = EdsGetPropertyData(
            imageRef_,
            kEdsPropID_Evf_CoordinateSystem,
            0,
            sizeof (coordinateSystem),
            &coordinateSystem
        );
        if (error != EDS_ERR_OK) {
            return env.Undefined();
        }
        Napi::Object size = Napi::Object::New(env);
        size.Set("width", Napi::Number::New(env, coordinateSystem.width));
        size.Set("height", Napi::Number::New(env, coordinateSystem.height));
        return size;
    }

    Napi::Value LiveViewImage::GetHistogram(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();
        if (imageRef_ == nullptr) {
            return env.Undefined();
        }
        Napi::Uint32Array y = Napi::TypedArrayOf<uint32_t>::New(env, 256);
        Napi::Uint32Array r = Napi::TypedArrayOf<uint32_t>::New(env, 256);
        Napi::Uint32Array g = Napi::TypedArrayOf<uint32_t>::New(env, 256);
        Napi::Uint32Array b = Napi::TypedArrayOf<uint32_t>::New(env, 256);
        auto propertySize = 256 * sizeof(EdsUInt32);

        EdsError error = EdsGetPropertyData(
            imageRef_, kEdsPropID_Evf_HistogramY, 0, propertySize, y.Data()
        );
        if (error != EDS_ERR_OK) {
            return env.Undefined();
        }
        error = EdsGetPropertyData(
            imageRef_, kEdsPropID_Evf_HistogramR, 0, propertySize, r.Data()
        );
        if (error != EDS_ERR_OK) {
            return env.Undefined();
        }
        error = EdsGetPropertyData(
            imageRef_, kEdsPropID_Evf_HistogramG, 0, propertySize, g.Data()
        );
        if (error != EDS_ERR_OK) {
            return env.Undefined();
        }
        error = EdsGetPropertyData(
            imageRef_, kEdsPropID_Evf_HistogramB, 0, propertySize, b.Data()
        );
        if (error != EDS_ERR_OK) {
            return env.Undefined();
        }
        Napi::Object histogram = Napi::Object::New(env);
        histogram.Set("y", y);
        histogram.Set("r", r);
        histogram.Set("g", g);
        histogram.Set("b", b);
        return histogram;
    }

    Napi::Value LiveViewImage::GetHistogramStatus(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();
        if (imageRef_ == nullptr) {
            return Option::NewInstance(env, kEdsPropID_Evf_HistogramStatus, 0);
        }
        EdsUInt32 status = 0;
        EdsError error = EdsGetPropertyData(
            imageRef_,
            kEdsPropID_Evf_HistogramStatus,
            0,
            sizeof (status),
            &status
        );
        if (error != EDS_ERR_OK) {
            status = 0;
        }
        return Option::NewInstance(env, kEdsPropID_Evf_HistogramStatus, status);
    }

    Napi::Value LiveViewImage::GetPosition(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();
        if (imageRef_ == nullptr) {
            return env.Undefined();
        }
        EdsPoint imagePosition;
        EdsError error = EdsGetPropertyData(
            imageRef_,
            kEdsPropID_Evf_ImagePosition,
            0,
            sizeof (imagePosition),
            &imagePosition
        );
        if (error != EDS_ERR_OK) {
            return env.Undefined();
        }
        Napi::Object position = Napi::Object::New(env);
        position.Set("left", Napi::Number::New(env, imagePosition.x));
        position.Set("top", Napi::Number::New(env, imagePosition.y));
        return position;
    }


    Napi::Value LiveViewImage::GetVisibleArea(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();
        if (imageRef_ == nullptr) {
            return env.Undefined();
        }
        EdsRect visibleArea;
        EdsError error = EdsGetPropertyData(
            imageRef_,
            kEdsPropID_Evf_VisibleRect,
            0,
            sizeof (visibleArea),
            &visibleArea
        );
        if (error != EDS_ERR_OK) {
            return env.Undefined();
        }
        Napi::Object area = Napi::Object::New(env);
        area.Set("left", Napi::Number::New(env, visibleArea.point.x));
        area.Set("top", Napi::Number::New(env, visibleArea.point.y));
        area.Set("width", Napi::Number::New(env, visibleArea.size.width));
        area.Set("height", Napi::Number::New(env, visibleArea.size.height));
        return area;
    }

    Napi::Value LiveViewImage::GetZoom(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();
        if (imageRef_ == nullptr) {
            return Option::NewInstance(env, kEdsPropID_Evf_Zoom, 0);
        }
        EdsUInt32 zoomFactor = 0;
        EdsError error = EdsGetPropertyData(
            imageRef_,
            kEdsPropID_Evf_Zoom,
            0,
            sizeof (zoomFactor),
            &zoomFactor
        );
        if (error != EDS_ERR_OK) {
            zoomFactor = 0;
        }
        return Option::NewInstance(env, kEdsPropID_Evf_Zoom, zoomFactor);
    }

    Napi::Value LiveViewImage::GetZoomArea(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();
        if (imageRef_ == nullptr) {
            return env.Undefined();
        }
        EdsRect zoomArea;
        EdsError error = EdsGetPropertyData(
            imageRef_,
            kEdsPropID_Evf_ZoomRect,
            0,
            sizeof (zoomArea),
            &zoomArea
        );
        if (error != EDS_ERR_OK) {
            return env.Undefined();
        }
        Napi::Object zoom = Napi::Object::New(env);
        zoom.Set("left", Napi::Number::New(env, zoomArea.point.x));
        zoom.Set("top", Napi::Number::New(env, zoomArea.point.y));
        zoom.Set("width", Napi::Number::New(env, zoomArea.size.width));
        zoom.Set("height", Napi::Number::New(env, zoomArea.size.height));
        return zoom;
    }

    Napi::Value LiveViewImage::GetZoomPosition(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();
        if (imageRef_ == nullptr) {
            return env.Undefined();
        }
        EdsPoint zoomPosition;
        EdsError error = EdsGetPropertyData(
            imageRef_,
            kEdsPropID_Evf_ZoomPosition,
            0,
            sizeof (zoomPosition),
            &zoomPosition
        );
        if (error != EDS_ERR_OK) {
            return env.Undefined();
        }
        Napi::Object zoom = Napi::Object::New(env);
        zoom.Set("left", Napi::Number::New(env, zoomPosition.x));
        zoom.Set("top", Napi::Number::New(env, zoomPosition.y));
        return zoom;
    }

    Napi::Value LiveViewImage::ToStringTag(const Napi::CallbackInfo &info) {
        return Napi::String::New(info.Env(), LiveViewImage::JSClassName);
    }

    Napi::Value LiveViewImage::Inspect(const Napi::CallbackInfo &info) {
        auto env = info.Env();
        auto stylize = info[1].As<Napi::Object>().Get("stylize").As<Napi::Function>();
        std::string output = stylize.Call(
            {Napi::String::New(env, LiveViewImage::JSClassName), Napi::String::New(env, "special")}
        ).As<Napi::String>().Utf8Value();
        return Napi::String::New(env, output);
    }

    Napi::Object LiveViewImage::NewInstance(Napi::Env env, EdsCameraRef edsCameraRef) {
        Napi::EscapableHandleScope scope(env);
        Napi::Object wrap = JSConstructor().New({Napi::External<EdsImageRef>::New(env, &edsCameraRef)});
        return scope.Escape(napi_value(wrap)).ToObject();
    }

    void LiveViewImage::Init(Napi::Env env, Napi::Object exports) {
        Napi::HandleScope scope(env);

        Napi::Function func = DefineClass(
            env,
            LiveViewImage::JSClassName,
            {
                InstanceAccessor<&LiveViewImage::ToStringTag>(Napi::Symbol::WellKnown(env, "toStringTag")),
                InstanceMethod(GetPublicSymbol(env, "nodejs.util.inspect.custom"), &LiveViewImage::Inspect),

                InstanceAccessor<&LiveViewImage::GetCoordinateSystem>("coordinateSystem"),
                InstanceAccessor<&LiveViewImage::GetHistogram>("histogram"),
                InstanceAccessor<&LiveViewImage::GetHistogramStatus>("histogramStatus"),
                InstanceAccessor<&LiveViewImage::GetPosition>("position"),
                InstanceAccessor<&LiveViewImage::GetVisibleArea>("visibleArea"),
                InstanceAccessor<&LiveViewImage::GetZoom>("zoom"),
                InstanceAccessor<&LiveViewImage::GetZoomArea>("zoomArea"),
                InstanceAccessor<&LiveViewImage::GetZoomPosition>("zoomPosition"),

                InstanceMethod("getDataURL", &LiveViewImage::GetDataURL),
                InstanceMethod("getJPEGBuffer", &LiveViewImage::GetJPEGBuffer),
                InstanceMethod("getBlob", &LiveViewImage::GetJPEGBuffer),
            }
        );
        JSConstructor(&func);
    }
} // CameraApi
