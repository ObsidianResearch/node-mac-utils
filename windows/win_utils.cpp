#include <napi.h>
#include <windows.h>
#include "AudioProcessMonitor.h"

// Gets a list of processes that are accessing input (microphone) - original interface
Napi::Value GetRunningInputAudioProcesses(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  try {
    std::vector<std::string> processes = GetAudioInputProcesses();

    Napi::Array result = Napi::Array::New(env);
    for (size_t i = 0; i < processes.size(); i++) {
      result.Set(i, Napi::String::New(env, processes[i]));
    }

    return result;
  } catch (const std::exception& e) {
    Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
    return env.Null();
  }
}

// Gets processes accessing microphone with structured result
Napi::Value GetProcessesAccessingMicrophoneWithResult(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  try {
    AudioProcessResult result = GetProcessesAccessingMicrophoneWithResult();

    // Create a JavaScript object to represent the AudioProcessResult
    Napi::Object resultObj = Napi::Object::New(env);
    if (!result.success) {
      // Set error information
      resultObj.Set("success", Napi::Boolean::New(env, false));
      resultObj.Set("error", Napi::String::New(env, result.errorMessage));
      resultObj.Set("code", Napi::Number::New(env, result.errorCode));
      resultObj.Set("domain", Napi::String::New(env, "AudioProcessMonitor"));
      resultObj.Set("processes", Napi::Array::New(env));
    } else {
      // Set success information
      resultObj.Set("success", Napi::Boolean::New(env, true));
      resultObj.Set("error", env.Null());

      // Convert processes array
      Napi::Array processesArray = Napi::Array::New(env);
      for (size_t i = 0; i < result.processes.size(); i++) {
        processesArray.Set(i, Napi::String::New(env, result.processes[i]));
      }
      resultObj.Set("processes", processesArray);
    }

    return resultObj;
  } catch (const std::exception& e) {
    Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
    return env.Null();
  }
}

// Gets a list of processes that are using speakers/render devices
Napi::Value GetRenderProcessesWithResult(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  try {
    RenderProcessResult result = GetRenderProcessesWithResult();

    // Create a JavaScript object to represent the RenderProcessResult
    Napi::Object resultObj = Napi::Object::New(env);
    if (!result.success) {
      // Set error information
      resultObj.Set("success", Napi::Boolean::New(env, false));
      resultObj.Set("error", Napi::String::New(env, result.errorMessage));
      resultObj.Set("code", Napi::Number::New(env, result.errorCode));
      resultObj.Set("domain", Napi::String::New(env, "RenderProcessMonitor"));
      resultObj.Set("processes", Napi::Array::New(env));
    } else {
      // Set success information
      resultObj.Set("success", Napi::Boolean::New(env, true));
      resultObj.Set("error", env.Null());

      // Convert processes array
      Napi::Array processesArray = Napi::Array::New(env);
      for (size_t i = 0; i < result.processes.size(); i++) {
        Napi::Object processObj = Napi::Object::New(env);
        processObj.Set("processName", Napi::String::New(env, result.processes[i].processName));
        processObj.Set("processId", Napi::Number::New(env, result.processes[i].processId));
        processObj.Set("deviceId", Napi::String::New(env, result.processes[i].deviceId));
        processObj.Set("deviceName", Napi::String::New(env, result.processes[i].deviceName));
        if (result.processes[i].hasPeak) {
          processObj.Set("peak", Napi::Number::New(env, result.processes[i].peak));
        }
        processObj.Set("isActive", Napi::Boolean::New(env, result.processes[i].isActive));
        processesArray.Set(i, processObj);
      }
      resultObj.Set("processes", processesArray);
    }

    return resultObj;
  } catch (const std::exception& e) {
    Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
    return env.Null();
  }
}

static Napi::Value RenderDeviceToJs(Napi::Env env, const RenderDeviceInfo& device) {
  if (device.deviceId.empty()) return env.Null();
  Napi::Object deviceObj = Napi::Object::New(env);
  deviceObj.Set("deviceId", Napi::String::New(env, device.deviceId));
  deviceObj.Set("deviceName", Napi::String::New(env, device.deviceName));
  return deviceObj;
}

// Gets the default render devices: { console, communications }, each { deviceId, deviceName } or null
Napi::Value GetDefaultRenderDevice(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();

  try {
    DefaultRenderDeviceResult result = GetDefaultRenderDevice();

    Napi::Object resultObj = Napi::Object::New(env);
    if (!result.success) {
      resultObj.Set("success", Napi::Boolean::New(env, false));
      resultObj.Set("error", Napi::String::New(env, result.errorMessage));
      resultObj.Set("code", Napi::Number::New(env, result.errorCode));
      resultObj.Set("domain", Napi::String::New(env, "RenderProcessMonitor"));
      resultObj.Set("console", env.Null());
      resultObj.Set("communications", env.Null());
    } else {
      resultObj.Set("success", Napi::Boolean::New(env, true));
      resultObj.Set("error", env.Null());
      resultObj.Set("console", RenderDeviceToJs(env, result.console));
      resultObj.Set("communications", RenderDeviceToJs(env, result.communications));
    }

    return resultObj;
  } catch (const std::exception& e) {
    Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
    return env.Null();
  }
}

// Initialize the module exports
Napi::Object Init(Napi::Env env, Napi::Object exports) {
  Napi::Value (*originalAudioProcessesFunc)(const Napi::CallbackInfo&) = GetRunningInputAudioProcesses;
  Napi::Value (*microphoneAccessFunc)(const Napi::CallbackInfo&) = GetProcessesAccessingMicrophoneWithResult;
  Napi::Value (*renderProcessesFunc)(const Napi::CallbackInfo&) = GetRenderProcessesWithResult;
  Napi::Value (*defaultRenderDeviceFunc)(const Napi::CallbackInfo&) = GetDefaultRenderDevice;

  exports.Set("getRunningInputAudioProcesses",
              Napi::Function::New(env, originalAudioProcessesFunc));
  exports.Set("getProcessesAccessingMicrophoneWithResult",
              Napi::Function::New(env, microphoneAccessFunc));
  exports.Set("getProcessesAccessingSpeakersWithResult",
              Napi::Function::New(env, renderProcessesFunc));
  exports.Set("getDefaultRenderDevice", Napi::Function::New(env, defaultRenderDeviceFunc));

  return exports;
}

NODE_API_MODULE(win_utils, Init)