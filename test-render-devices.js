// Smoke test for the render-device functions (PM-4211): both answer the documented shape and never throw. A machine
// without audio devices (a CI runner) may answer empty lists and null devices; that is a pass.
const assert = require("assert");
const utils = require("./index.js");

const isWindows = process.platform === "win32";

const speakers = utils.getProcessesAccessingSpeakersWithResult();
console.log("getProcessesAccessingSpeakersWithResult:", JSON.stringify(speakers));
assert.strictEqual(typeof speakers.success, "boolean");
assert.ok(Array.isArray(speakers.processes), "processes is an array");
if (speakers.success) assert.strictEqual(speakers.error, null);
for (const session of speakers.processes) {
  assert.strictEqual(typeof session.processName, "string");
  assert.strictEqual(typeof session.processId, "number");
  if (isWindows) {
    assert.strictEqual(typeof session.deviceId, "string", "deviceId on Windows");
    assert.strictEqual(typeof session.deviceName, "string", "deviceName on Windows");
    if ("peak" in session) {
      assert.ok(session.peak >= 0 && session.peak <= 1, "peak is 0..1");
    }
  }
}

const isDevice = (device) =>
  device === null ||
  (typeof device.deviceId === "string" && device.deviceId.length > 0 && typeof device.deviceName === "string");

const defaults = utils.getDefaultRenderDevice();
console.log("getDefaultRenderDevice:", JSON.stringify(defaults));
assert.ok(isDevice(defaults.console), "console is a device or null");
assert.ok(isDevice(defaults.communications), "communications is a device or null");
if (isWindows) {
  // A runner with no speaker has none for either role, which is still a success.
  assert.strictEqual(defaults.success, true, `getDefaultRenderDevice failed: ${defaults.error}`);
  assert.strictEqual(defaults.error, null);
} else {
  assert.deepStrictEqual(defaults, { success: false, error: "unsupported", console: null, communications: null });
}

console.log("render-device smoke test passed on", process.platform);
