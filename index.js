let platform_utils;

const noopPlatformUtils = {
  getRunningInputAudioProcesses: () => {
    return ["", ""];
  },
  getProcessesAccessingMicrophoneWithResult: () => {
    return {
      success: true,
      error: null,
      processes: ["", ""],
    };
  },
  getProcessesAccessingSpeakersWithResult: () => {
    return {
      success: true,
      error: null,
      processes: [],
    };
  },
  getDefaultRenderDevice: () => {
    return {
      success: true,
      error: null,
      console: null,
      communications: null,
    };
  },
};

if (process.platform === "darwin") {
  platform_utils = require("bindings")("mac_utils.node");
} else if (process.platform === "win32") {
  platform_utils = require("bindings")("win_utils.node");
} else {
  console.log("node-mac-utils Unsupported platform:", process.platform);
  platform_utils = noopPlatformUtils;
}

module.exports = {
  // Common exports that work on all platforms
  getRunningInputAudioProcesses: platform_utils.getRunningInputAudioProcesses,
  getProcessesAccessingMicrophoneWithResult:
    platform_utils.getProcessesAccessingMicrophoneWithResult,
  getProcessesAccessingSpeakersWithResult:
    platform_utils.getProcessesAccessingSpeakersWithResult,
  // Windows only: undefined on macOS
  getDefaultRenderDevice: platform_utils.getDefaultRenderDevice,
  INFO_ERROR_CODE: 1,
  ERROR_DOMAIN: "com.MicrophoneUsageMonitor",

  // Mac-specific exports
  ...(process.platform === "darwin"
    ? {
        makeKeyAndOrderFront: platform_utils.makeKeyAndOrderFront,
        startMonitoringMic: platform_utils.startMonitoringMic,
        stopMonitoringMic: platform_utils.stopMonitoringMic,
      }
    : {}),
};
