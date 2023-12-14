#include "aspect.h"
#include "utility.h"
#include <unordered_map>
#include <iostream>

namespace CameraApi {

    const LabelMap &AspectLabels() {
        static const LabelMap map = {
            {0x00000000, "full"},
            {0x00000001, "square"},
            {0x00000002, "4:3"},
            {0x00000007, "16:9"},
            {0x0000000d, "apsc"},
        };
        return map;
    }

    Aspect::Aspect(const Napi::CallbackInfo &info)
        : Napi::ObjectWrap<Aspect>(info) {

        Napi::Env env = info.Env();
        Napi::HandleScope scope(env);

        if (info.Length() > 0 && info[0].IsNumber()) {
            value_ = info[0].As<Napi::Number>().Uint32Value();
        } else {
            throw Napi::TypeError::New(
                info.Env(), "Argument 0 must be a property value."
            );
        }

    }
    
    Napi::Value Aspect::GetLabel(const Napi::CallbackInfo &info) {
        std::string label;
        auto labels = AspectLabels();
        if (labels.find(value_) != labels.end()) {
            label.append(labels[value_]);
        } else {
            label.append(CodeToHexLabel(value_));
        }
        return Napi::String::New(info.Env(), label);

        // return Napi::String::New(info.Env(), GetLabelForValue(value_));
    }

    Napi::Value Aspect::GetValue(const Napi::CallbackInfo &info) {
        return Napi::Number::New(info.Env(), value_);
    }

    Napi::Value Aspect::GetPrimitive(const Napi::CallbackInfo &info) {
        std::uint32_t  hint = info[0].As<Napi::Number>().Uint32Value();
        return GetValue(info);
        if (info.Length() > 0) {
            std::uint32_t  hint = info[0].As<Napi::Number>().Uint32Value();
            return GetValue(info);
        }
        return info.Env().Null();
    }

    Napi::Value Aspect::ToJSON(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();
        Napi::Object Json = Napi::Object::New(env);
        Json.Set("label", GetLabel(info));
        Json.Set("value", GetValue(info));
        return Json;
    }

    Napi::Value Aspect::ToStringTag(const Napi::CallbackInfo &info) {
        return Napi::String::New(info.Env(), Aspect::JSClassName);
    }

    Napi::Value Aspect::Inspect(const Napi::CallbackInfo &info) {
        auto env = info.Env();
        auto stylize = info[1].As<Napi::Object>().Get("stylize").As<Napi::Function>();
        std::string output = stylize.Call(
            {Napi::String::New(env, Aspect::JSClassName), Napi::String::New(env, "special")}
        ).As<Napi::String>().Utf8Value();
        output.append(" <");
        output.append(
            stylize.Call(
                {
                    GetLabel(info),
                    (value_ == 0)
                    ? Napi::String::New(env, "string")
                    : Napi::String::New(env, "number")
                }
            ).As<Napi::String>().Utf8Value()
        );
        output.append(">");
        return Napi::String::New(env, output);
    }




    Napi::Object Aspect::NewInstance(Napi::Env env, EdsUInt32 value) {
        Napi::EscapableHandleScope scope(env);
        Napi::Object wrap = JSConstructor().New(
            {
                Napi::Number::New(env, value)
            }
        );
        return scope.Escape(napi_value(wrap)).ToObject();
    }

    void Aspect::Init(Napi::Env env, Napi::Object exports) {
        Napi::HandleScope scope(env);

        Napi::Object FileTypes = Napi::Object::New(env);
        for (const auto &it: AspectLabels()) {
            FileTypes.Set(
                it.second, Napi::Number::New(env, it.first)
            );
        }

        std::vector<PropertyDescriptor> properties = {
            InstanceAccessor<&Aspect::GetLabel>("label"),
            InstanceAccessor<&Aspect::GetValue>("value"),

            InstanceMethod(Napi::Symbol::WellKnown(env, "toPrimitive"), &Aspect::GetPrimitive),
            InstanceMethod("toJSON", &Aspect::ToJSON),

            InstanceAccessor<&Aspect::ToStringTag>(Napi::Symbol::WellKnown(env, "toStringTag")),
            InstanceMethod(GetPublicSymbol(env, "nodejs.util.inspect.custom"), &Aspect::Inspect),

            // StaticValue("ID", IDs, napi_enumerable),
            // StaticValue("Values", Values, napi_enumerable)
        };

        Napi::Function func = DefineClass(env, Aspect::JSClassName, properties);
        JSConstructor(&func);

        exports.Set(Aspect::JSClassName, func);
    }
}
