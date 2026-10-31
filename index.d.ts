export interface AudioProcessResult {
  success: boolean;
  error: string | null;
  code?: number;
  domain?: string;
  processes: string[];
}

export interface RenderProcessInfo {
  /** The executable's file name, e.g. `ms-teams.exe`. */
  processName: string;
  processId: number;
  /** `IMMDevice::GetId()` of the render endpoint the session plays on (Windows). */
  deviceId?: string;
  /** The endpoint's friendly name, e.g. `Headset Earphone (Jabra Evolve2 65)` (Windows). */
  deviceName?: string;
  /** The session's current peak level, 0..1; absent when the session has no meter (Windows). */
  peak?: number;
  isActive?: boolean;
}

export interface RenderProcessResult {
  success: boolean;
  error: string | null;
  code?: number;
  domain?: string;
  processes: RenderProcessInfo[];
}

export interface RenderDevice {
  deviceId: string;
  deviceName: string;
}

export interface DefaultRenderDeviceResult {
  success: boolean;
  /** `'unsupported'` on macOS and Linux. */
  error: string | null;
  code?: number;
  /** The default speaker (`eConsole`), null when there is none. */
  console: RenderDevice | null;
  /** The default communications speaker (`eCommunications`), null when there is none. */
  communications: RenderDevice | null;
}

export function getRunningInputAudioProcesses(): string[];
export function getProcessesAccessingMicrophoneWithResult(): AudioProcessResult;
/** Active, unmuted render sessions. Windows only; macOS and Linux answer an empty list. */
export function getProcessesAccessingSpeakersWithResult(): RenderProcessResult;
/** The default render devices. Windows only; macOS and Linux answer `{ success: false, error: 'unsupported' }`. */
export function getDefaultRenderDevice(): DefaultRenderDeviceResult;

export const INFO_ERROR_CODE: number;
export const ERROR_DOMAIN: string;

/** macOS only. */
export const makeKeyAndOrderFront: ((...args: unknown[]) => unknown) | undefined;
/** macOS only. */
export const startMonitoringMic: ((...args: unknown[]) => unknown) | undefined;
/** macOS only. */
export const stopMonitoringMic: ((...args: unknown[]) => unknown) | undefined;
