#pragma once
#include <string>
#include <vector>

struct AudioProcessResult {
    std::vector<std::string> processes;
    HRESULT errorCode;
    std::string errorMessage;
    bool success;

    AudioProcessResult() : errorCode(S_OK), success(true) {}
};

// Speaker/render process detection for Windows
struct RenderProcessInfo {
    std::string processName;
    DWORD processId;
    // IMMDevice::GetId of the render endpoint the session plays on, stable across renames.
    std::string deviceId;
    std::string deviceName;
    // IAudioMeterInformation::GetPeakValue of the session, 0..1; 0 when it can't be read.
    float peak;
    bool isActive;
};

struct RenderProcessResult {
    std::vector<RenderProcessInfo> processes;
    HRESULT errorCode;
    std::string errorMessage;
    bool success;

    RenderProcessResult() : errorCode(S_OK), success(true) {}
};

// Original function returning vector (restored)
std::vector<std::string> GetAudioInputProcesses();

// New function with structured result
AudioProcessResult GetProcessesAccessingMicrophoneWithResult();

// Speaker/render process detection
RenderProcessResult GetRenderProcessesWithResult();

// A render endpoint, empty id when there is no default for the role.
struct RenderDeviceInfo {
    std::string deviceId;
    std::string deviceName;
};

// The default render endpoints: eConsole (what "loopback" captures) and eCommunications (what call apps often use).
struct DefaultRenderDeviceResult {
    RenderDeviceInfo console;
    RenderDeviceInfo communications;
    HRESULT errorCode;
    std::string errorMessage;
    bool success;

    DefaultRenderDeviceResult() : errorCode(S_OK), success(true) {}
};

DefaultRenderDeviceResult GetDefaultRenderDevice();