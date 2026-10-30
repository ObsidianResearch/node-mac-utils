// AudioProcessMonitor.cpp
//

#include <windows.h>

#include <audiopolicy.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <iostream>
#include <psapi.h>
#include <string>
#include <vector>
#include "AudioProcessMonitor.h"
#include <Audioclient.h>
#include <unordered_set>
#include <functiondiscoverykeys_devpkey.h>

#pragma comment(lib, "Ole32.lib")

// Function to get process executable path from PID
static std::string GetProcessExecutablePath(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processID);
    if (!hProcess) return "Unknown";

    WCHAR path[MAX_PATH];
    DWORD size = MAX_PATH;
    
    if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
        CloseHandle(hProcess);
        // Convert wide string to regular string
        int strSize = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
        std::string result(strSize, 0);
        WideCharToMultiByte(CP_UTF8, 0, path, -1, &result[0], strSize, nullptr, nullptr);
        if (!result.empty() && result.back() == 0) {
            result.pop_back();
        }
        return result;
    }

    CloseHandle(hProcess);
    return "Unknown";
}

// UTF-16 to UTF-8
static std::string WideToUtf8(const wchar_t* wide) {
    if (!wide) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return "";
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &result[0], size, nullptr, nullptr);
    result.pop_back();  // the terminating null
    return result;
}

static std::string GetDeviceId(IMMDevice* pDevice) {
    LPWSTR pwszId = nullptr;
    if (FAILED(pDevice->GetId(&pwszId)) || !pwszId) return "";
    std::string id = WideToUtf8(pwszId);
    CoTaskMemFree(pwszId);
    return id;
}

static std::string GetDeviceFriendlyName(IMMDevice* pDevice) {
    std::string deviceName = "Unknown Device";
    IPropertyStore* pProps = nullptr;
    if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
        PROPVARIANT varName;
        PropVariantInit(&varName);
        if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName)) && varName.vt == VT_LPWSTR) {
            deviceName = WideToUtf8(varName.pwszVal);
        }
        PropVariantClear(&varName);
        pProps->Release();
    }
    return deviceName;
}

// New function with structured result - checks ALL capture devices
AudioProcessResult GetProcessesAccessingMicrophoneWithResult() {
    AudioProcessResult result;
    std::unordered_set<std::string> seen;  // Track unique strings
    HRESULT hr = CoInitialize(nullptr);

    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to initialize COM";
        result.success = false;
        return result;
    }

    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDeviceCollection* pCollection = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to create device enumerator";
        result.success = false;
        CoUninitialize();
        return result;
    }

    // Get ALL active capture (microphone) devices
    hr = pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to enumerate audio endpoints";
        result.success = false;
        pEnumerator->Release();
        CoUninitialize();
        return result;
    }

    UINT deviceCount = 0;
    pCollection->GetCount(&deviceCount);

    // Check each active capture device
    for (UINT deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++) {
        IMMDevice* pDevice = nullptr;
        hr = pCollection->Item(deviceIndex, &pDevice);
        if (FAILED(hr)) continue;

        // Check if this device has active audio
        bool isPeakValueActive = false;
        IAudioMeterInformation* pMeter = nullptr;
        hr = pDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr, (void**)&pMeter);
        if (SUCCEEDED(hr)) {
            float peakValue = 0.0f;
            pMeter->GetPeakValue(&peakValue);
            isPeakValueActive = (peakValue > 0.0f);
            pMeter->Release();
        }

        bool isPaddingActive = false;
        IAudioClient* pAudioClient = nullptr;
        hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
        if (SUCCEEDED(hr)) {
            UINT32 padding = 0;
            hr = pAudioClient->GetCurrentPadding(&padding);
            isPaddingActive = SUCCEEDED(hr) && padding > 0;
            pAudioClient->Release();
        }

        // Skip this device if no active audio
        if (!isPeakValueActive && !isPaddingActive) {
            pDevice->Release();
            continue;
        }

        IAudioSessionManager2* pSessionManager = nullptr;
        hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pSessionManager);
        if (SUCCEEDED(hr)) {
            IAudioSessionEnumerator* pSessionEnum = nullptr;
            hr = pSessionManager->GetSessionEnumerator(&pSessionEnum);
            if (SUCCEEDED(hr)) {
                int sessionCount = 0;
                pSessionEnum->GetCount(&sessionCount);

                for (int i = 0; i < sessionCount; i++) {
                    IAudioSessionControl* pSessionControl = nullptr;
                    hr = pSessionEnum->GetSession(i, &pSessionControl);
                    if (FAILED(hr)) continue;

                    IAudioSessionControl2* pSessionControl2 = nullptr;
                    hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
                    if (SUCCEEDED(hr)) {
                        DWORD processID = 0;
                        pSessionControl2->GetProcessId(&processID);

                        AudioSessionState state;
                        pSessionControl2->GetState(&state);

                        if (processID != 0 && state == AudioSessionStateActive) {
                            std::string processPath = GetProcessExecutablePath(processID);

                            // Only insert if not already seen
                            if (seen.insert(processPath).second) {
                                result.processes.push_back(processPath);
                            }
                        }
                        pSessionControl2->Release();
                    }
                    pSessionControl->Release();
                }
                pSessionEnum->Release();
            }
            pSessionManager->Release();
        }

        pDevice->Release();
    }

    // Cleanup
    pCollection->Release();
    pEnumerator->Release();
    CoUninitialize();

    return result;
}

// Checks ALL capture devices for processes using audio input
std::vector<std::string> GetAudioInputProcesses() {
    std::vector<std::string> results;
    std::unordered_set<std::string> seen;  // Track unique strings
    HRESULT hr = CoInitialize(nullptr);

    if (FAILED(hr)) {
        return results;
    }

    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDeviceCollection* pCollection = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        CoUninitialize();
        return results;
    }

    // Get ALL active capture (microphone) devices
    hr = pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        pEnumerator->Release();
        CoUninitialize();
        return results;
    }

    UINT deviceCount = 0;
    pCollection->GetCount(&deviceCount);

    // Check each active capture device
    for (UINT deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++) {
        IMMDevice* pDevice = nullptr;
        hr = pCollection->Item(deviceIndex, &pDevice);
        if (FAILED(hr)) continue;

        // Check if this device has active audio
        bool isActive = false;
        IAudioMeterInformation* pMeter = nullptr;
        hr = pDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr, (void**)&pMeter);
        if (SUCCEEDED(hr)) {
            float peakValue = 0.0f;
            pMeter->GetPeakValue(&peakValue);
            isActive = (peakValue > 0.0f);
            pMeter->Release();
        }

        // Skip this device if no active audio
        if (!isActive) {
            pDevice->Release();
            continue;
        }

        IAudioSessionManager2* pSessionManager = nullptr;
        hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pSessionManager);
        if (SUCCEEDED(hr)) {
            IAudioSessionEnumerator* pSessionEnum = nullptr;
            hr = pSessionManager->GetSessionEnumerator(&pSessionEnum);
            if (SUCCEEDED(hr)) {
                int sessionCount = 0;
                pSessionEnum->GetCount(&sessionCount);

                for (int i = 0; i < sessionCount; i++) {
                    IAudioSessionControl* pSessionControl = nullptr;
                    hr = pSessionEnum->GetSession(i, &pSessionControl);
                    if (FAILED(hr)) continue;

                    IAudioSessionControl2* pSessionControl2 = nullptr;
                    hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
                    if (SUCCEEDED(hr)) {
                        DWORD processID = 0;
                        pSessionControl2->GetProcessId(&processID);

                        AudioSessionState state;
                        pSessionControl2->GetState(&state);

                        if (processID != 0 && state == AudioSessionStateActive) {
                            std::string processPath = GetProcessExecutablePath(processID);

                            // Only insert if not already seen
                            if (seen.insert(processPath).second) {
                                results.push_back(processPath);
                            }
                        }
                        pSessionControl2->Release();
                    }
                    pSessionControl->Release();
                }
                pSessionEnum->Release();
            }
            pSessionManager->Release();
        }

        pDevice->Release();
    }

    // Cleanup
    pCollection->Release();
    pEnumerator->Release();
    CoUninitialize();

    return results;
}

// Speaker/render process detection - separate from microphone monitoring
RenderProcessResult GetRenderProcessesWithResult() {
    RenderProcessResult result;
    HRESULT hr = CoInitialize(nullptr);

    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to initialize COM";
        result.success = false;
        return result;
    }

    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDeviceCollection* pCollection = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to create device enumerator";
        result.success = false;
        CoUninitialize();
        return result;
    }

    // Get ALL active render (speaker) devices
    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to enumerate audio endpoints";
        result.success = false;
        pEnumerator->Release();
        CoUninitialize();
        return result;
    }

    UINT deviceCount = 0;
    pCollection->GetCount(&deviceCount);

    // Check each active render device
    for (UINT deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++) {
        IMMDevice* pDevice = nullptr;
        hr = pCollection->Item(deviceIndex, &pDevice);
        if (FAILED(hr)) continue;

        std::string deviceId = GetDeviceId(pDevice);
        std::string deviceName = GetDeviceFriendlyName(pDevice);

        IAudioSessionManager2* pSessionManager = nullptr;
        hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pSessionManager);
        if (SUCCEEDED(hr)) {
            IAudioSessionEnumerator* pSessionEnum = nullptr;
            hr = pSessionManager->GetSessionEnumerator(&pSessionEnum);
            if (SUCCEEDED(hr)) {
                int sessionCount = 0;
                pSessionEnum->GetCount(&sessionCount);

                for (int i = 0; i < sessionCount; i++) {
                    IAudioSessionControl* pSessionControl = nullptr;
                    hr = pSessionEnum->GetSession(i, &pSessionControl);
                    if (FAILED(hr)) continue;

                    IAudioSessionControl2* pSessionControl2 = nullptr;
                    hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
                    if (SUCCEEDED(hr)) {
                        DWORD processId = 0;
                        pSessionControl2->GetProcessId(&processId);

                        AudioSessionState state;
                        pSessionControl2->GetState(&state);

                        // Check if session is active and not muted
                        ISimpleAudioVolume* pVolume = nullptr;
                        bool isActiveSession = false;
                        hr = pSessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVolume);
                        if (SUCCEEDED(hr)) {
                            BOOL isMuted = FALSE;
                            pVolume->GetMute(&isMuted);

                            // For render sessions, active state + not muted = active
                            isActiveSession = (state == AudioSessionStateActive && !isMuted);
                            pVolume->Release();
                        }

                        if (processId != 0 && isActiveSession) {
                            RenderProcessInfo info;
                            info.processId = processId;
                            info.processName = GetProcessExecutablePath(processId);

                            // Extract filename from path
                            size_t lastSlash = info.processName.find_last_of("\\");
                            if (lastSlash != std::string::npos) {
                                info.processName = info.processName.substr(lastSlash + 1);
                            }

                            info.deviceId = deviceId;
                            info.deviceName = deviceName;
                            info.peak = 0.0f;
                            IAudioMeterInformation* pMeter = nullptr;
                            if (SUCCEEDED(pSessionControl->QueryInterface(__uuidof(IAudioMeterInformation), (void**)&pMeter))) {
                                float peak = 0.0f;
                                if (SUCCEEDED(pMeter->GetPeakValue(&peak))) {
                                    info.peak = peak;
                                }
                                pMeter->Release();
                            }
                            info.isActive = true;
                            result.processes.push_back(info);
                        }

                        pSessionControl2->Release();
                    }
                    pSessionControl->Release();
                }
                pSessionEnum->Release();
            }
            pSessionManager->Release();
        }

        pDevice->Release();
    }

    // Cleanup
    pCollection->Release();
    pEnumerator->Release();
    CoUninitialize();

    return result;
}

// The default render endpoints for the console and communications roles
DefaultRenderDeviceResult GetDefaultRenderDevice() {
    DefaultRenderDeviceResult result;
    HRESULT hr = CoInitialize(nullptr);

    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to initialize COM";
        result.success = false;
        return result;
    }

    IMMDeviceEnumerator* pEnumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to create device enumerator";
        result.success = false;
        CoUninitialize();
        return result;
    }

    // E_NOTFOUND (no render device at all) leaves the role empty rather than failing the call.
    ERole roles[] = {eConsole, eCommunications};
    RenderDeviceInfo* targets[] = {&result.console, &result.communications};
    for (int i = 0; i < 2; i++) {
        IMMDevice* pDevice = nullptr;
        if (SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(eRender, roles[i], &pDevice))) {
            targets[i]->deviceId = GetDeviceId(pDevice);
            targets[i]->deviceName = GetDeviceFriendlyName(pDevice);
            pDevice->Release();
        }
    }

    pEnumerator->Release();
    CoUninitialize();

    return result;
}
