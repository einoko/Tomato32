import { ESPLoader, Transport } from "esptool-js";
import JSZip from "jszip";
import "@fontsource-variable/inter/wght.css";
import "./style.css";

const BASE_URL = import.meta.env.BASE_URL;
const REPOSITORY_RELEASES = "https://github.com/einoko/Tomato32/releases";
const BOARD_ID = "waveshare-esp32-s3-touch-lcd-3.49";
const CHIP_ID = "ESP32-S3";
const FLASH_LIMIT = 16 * 1024 * 1024;
const MAX_ARCHIVE_BYTES = 20 * 1024 * 1024;
const EXPECTED_ADDRESSES = {
  fullInstall: new Set([0x0, 0x8000, 0x10000, 0x830000]),
  appUpdate: new Set([0x10000]),
};

const ui = {
  browserStatus: document.querySelector("#browser-status"),
  browserStatusTitle: document.querySelector("#browser-status-title"),
  browserStatusDetail: document.querySelector("#browser-status-detail"),
  connectionHelpButton: document.querySelector("#connection-help-button"),
  connectionHelpPanel: document.querySelector("#connection-help-panel"),
  releaseSelect: document.querySelector("#release-select"),
  releaseNote: document.querySelector("#release-note"),
  selectedReleaseLink: document.querySelector("#selected-release-link"),
  modeRadios: [...document.querySelectorAll('input[name="install-mode"]')],
  connectionState: document.querySelector("#connection-state"),
  connectButton: document.querySelector("#connect-button"),
  flashPanel: document.querySelector("#flash-panel"),
  flashButton: document.querySelector("#flash-button"),
  progressWrap: document.querySelector("#progress-wrap"),
  progress: document.querySelector("#flash-progress"),
  progressLabel: document.querySelector("#progress-label"),
  progressPercent: document.querySelector("#progress-percent"),
  resultBanner: document.querySelector("#result-banner"),
  resultTitle: document.querySelector("#result-title"),
  resultMessage: document.querySelector("#result-message"),
  resultFollowup: document.querySelector("#result-followup"),
  flashLog: document.querySelector("#flash-log"),
};

let releases = [];
let selectedRelease = null;
let preparedPackage = null;
let transport = null;
let loader = null;
let connectedChip = null;
let isBusy = false;
let logLines = [];
let prepareRequestId = 0;

function setBrowserStatus(title, detail = "", supported = true) {
  ui.browserStatus.classList.toggle("unsupported", !supported);
  ui.browserStatus.classList.toggle("supported", supported);
  ui.browserStatusTitle.textContent = title;
  ui.browserStatusDetail.textContent = detail;
  ui.browserStatusDetail.hidden = !detail;
}

function setConnection(message, state = "idle") {
  ui.connectionState.classList.toggle("connected", state === "connected");
  ui.connectionState.classList.toggle("error", state === "error");
  ui.connectionState.lastElementChild.textContent = message;
}

function setLog(message) {
  logLines.push(String(message));
  if (logLines.length > 180) logLines = logLines.slice(-180);
  ui.flashLog.textContent = logLines.join("\n");
  ui.flashLog.scrollTop = ui.flashLog.scrollHeight;
}

function setResult(title, message, isError = false) {
  ui.resultBanner.hidden = false;
  ui.resultBanner.classList.toggle("error", isError);
  ui.resultTitle.textContent = title;
  ui.resultMessage.textContent = message;
  ui.resultFollowup.hidden = isError;
}

function clearResult() {
  ui.resultBanner.hidden = true;
  ui.resultBanner.classList.remove("error");
}

function getMode() {
  return ui.modeRadios.find((radio) => radio.checked)?.value ?? "fullInstall";
}

function updateModeUI() {
  const mode = getMode();
  ui.modeRadios.forEach((radio) => {
    radio.closest(".mode-option").classList.toggle("selected", radio.checked);
  });
  ui.flashButton.textContent = mode === "fullInstall" ? "Install firmware" : "Update app";
}

function toggleConnectionHelp() {
  const expanded = ui.connectionHelpButton.getAttribute("aria-expanded") !== "true";
  ui.connectionHelpButton.setAttribute("aria-expanded", String(expanded));
  ui.connectionHelpPanel.setAttribute("aria-hidden", String(!expanded));
  ui.connectionHelpPanel.inert = !expanded;
  ui.connectionHelpPanel.classList.toggle("expanded", expanded);
}

function setBusy(value) {
  isBusy = value;
  ui.releaseSelect.disabled = value || releases.length === 0;
  ui.modeRadios.forEach((radio) => { radio.disabled = value; });
  ui.connectButton.disabled = value || !preparedPackage || Boolean(loader);
  ui.connectButton.textContent = loader ? "Connected" : "Connect device";
  ui.flashButton.disabled = value || !loader || !preparedPackage;
}

function safeReleaseDate(release) {
  if (release.local) return "Local development build";
  if (!release.published_at) return "Published release";
  const date = new Date(release.published_at);
  if (Number.isNaN(date.getTime())) return "Published release";
  return `Released ${new Intl.DateTimeFormat(undefined, { dateStyle: "medium" }).format(date)}`;
}

function updateReleaseMetadata() {
  const requestId = ++prepareRequestId;
  selectedRelease = releases.find((release) => release.tag === ui.releaseSelect.value) ?? null;
  preparedPackage = null;
  ui.flashPanel.hidden = true;
  clearResult();
  ui.selectedReleaseLink.href = selectedRelease?.html_url ?? REPOSITORY_RELEASES;
  ui.releaseNote.textContent = selectedRelease ? "Preparing firmware and checking its integrity…" : "No published stable releases are available yet.";
  setBusy(isBusy);
  if (selectedRelease) void prepareFirmware(selectedRelease, requestId);
}

async function loadReleaseList() {
  try {
    const response = await fetch(`${BASE_URL}releases.json`, { cache: "no-store" });
    if (!response.ok) throw new Error(`Release list returned HTTP ${response.status}.`);
    const data = await response.json();
    if (data.schemaVersion !== 1 || !Array.isArray(data.releases)) {
      throw new Error("The published release list has an unsupported format.");
    }
    releases = data.releases.filter((release) =>
      typeof release.tag === "string" &&
      /^v\d+\.\d+\.\d+$/.test(release.tag) &&
      release.assetPath === `firmware/tomato32-firmware-${release.tag}.zip` &&
      typeof release.assetSha256 === "string" &&
      /^[0-9a-f]{64}$/i.test(release.assetSha256) &&
      Number.isSafeInteger(release.assetSize) &&
      release.assetSize > 0 &&
      release.assetSize <= MAX_ARCHIVE_BYTES,
    );
    ui.releaseSelect.replaceChildren();
    if (releases.length === 0) {
      const option = document.createElement("option");
      option.textContent = "No stable releases published yet";
      ui.releaseSelect.append(option);
      ui.releaseNote.textContent = "The firmware installer will be ready when the first GitHub Release is published.";
      ui.releaseSelect.disabled = true;
      ui.connectButton.disabled = true;
      return;
    }
    for (const release of releases) {
      const option = document.createElement("option");
      option.value = release.tag;
      option.textContent = `${release.tag}${release.name && release.name !== release.tag ? ` — ${release.name}` : ""}`;
      ui.releaseSelect.append(option);
    }
    ui.releaseSelect.value = releases[0].tag;
    ui.releaseSelect.disabled = false;
    updateReleaseMetadata();
  } catch (error) {
    ui.releaseSelect.replaceChildren(new Option("Release list unavailable", ""));
    ui.releaseSelect.disabled = true;
    ui.releaseNote.textContent = "Could not load the release list. Check your connection or open GitHub Releases directly.";
    ui.selectedReleaseLink.href = REPOSITORY_RELEASES;
    ui.connectButton.disabled = true;
    setLog(`Release list error: ${error.message}`);
  }
}

async function sha256Hex(data) {
  const digest = await crypto.subtle.digest("SHA-256", data);
  return [...new Uint8Array(digest)].map((byte) => byte.toString(16).padStart(2, "0")).join("");
}

function validateManifest(manifest, release) {
  if (manifest?.schemaVersion !== 1) throw new Error("This firmware bundle has an unsupported manifest version.");
  if (manifest.version !== release.tag) throw new Error("The firmware bundle version does not match the selected release.");
  if (manifest.board?.id !== BOARD_ID) throw new Error("This firmware bundle is for a different board.");
  if (String(manifest.chip).toUpperCase() !== CHIP_ID) throw new Error("This firmware bundle is not for an ESP32-S3.");
  if (manifest.flash?.mode !== "dio" || manifest.flash?.frequency !== "80m" || manifest.flash?.size !== "16MB") {
    throw new Error("The firmware bundle has unexpected flash settings.");
  }
  for (const mode of Object.keys(EXPECTED_ADDRESSES)) {
    const entries = manifest[mode]?.images;
    if (!Array.isArray(entries) || entries.length !== EXPECTED_ADDRESSES[mode].size) {
      throw new Error(`The ${mode} image list is incomplete.`);
    }
    const addresses = new Set();
    for (const entry of entries) {
      if (typeof entry.path !== "string" || entry.path.startsWith("/") || entry.path.split("/").includes("..")) {
        throw new Error("The firmware manifest contains an invalid file path.");
      }
      if (!/^([0-9a-f]{64})$/i.test(entry.sha256)) throw new Error(`Missing SHA-256 hash for ${entry.path}.`);
      if (typeof entry.address !== "string" || !/^0x[0-9a-f]+$/i.test(entry.address)) {
        throw new Error(`Invalid flash address for ${entry.path}.`);
      }
      const address = Number.parseInt(entry.address, 16);
      if (!Number.isInteger(address) || address < 0 || address >= FLASH_LIMIT || addresses.has(address)) {
        throw new Error(`Invalid flash address for ${entry.path}.`);
      }
      addresses.add(address);
    }
    const expected = EXPECTED_ADDRESSES[mode];
    if (addresses.size !== expected.size || [...expected].some((address) => !addresses.has(address))) {
      throw new Error(`The ${mode} images do not match the supported Tomato32 flash layout.`);
    }
  }
}

async function prepareFirmware(release, requestId) {
  setBusy(isBusy);
  try {
    const expectedArchiveSize = Number(release.assetSize);
    if (!Number.isSafeInteger(expectedArchiveSize) || expectedArchiveSize <= 0) {
      throw new Error("The selected firmware bundle has invalid size metadata.");
    }
    if (expectedArchiveSize > MAX_ARCHIVE_BYTES) {
      throw new Error("The selected firmware bundle is unexpectedly large and was not loaded.");
    }
    const response = await fetch(`${BASE_URL}${release.assetPath}?sha256=${release.assetSha256}`, { cache: "force-cache" });
    if (requestId !== prepareRequestId) return;
    if (!response.ok) throw new Error(`Firmware download returned HTTP ${response.status}.`);
    const archiveBytes = await response.arrayBuffer();
    if (requestId !== prepareRequestId) return;
    if (archiveBytes.byteLength !== expectedArchiveSize) {
      throw new Error("The firmware ZIP has an unexpected size and was not flashed.");
    }
    const actualArchiveHash = await sha256Hex(archiveBytes);
    if (actualArchiveHash.toLowerCase() !== release.assetSha256.toLowerCase()) {
      throw new Error("The firmware ZIP failed its SHA-256 check. Please reload the page and try again.");
    }
    const zip = await JSZip.loadAsync(archiveBytes, { checkCRC32: true });
    const manifestFile = zip.file("manifest.json");
    if (!manifestFile) throw new Error("The firmware ZIP is missing its manifest.");
    const manifest = JSON.parse(await manifestFile.async("text"));
    validateManifest(manifest, release);
    if (requestId !== prepareRequestId) return;
    preparedPackage = { manifest, zip };
    ui.releaseNote.textContent = `${safeReleaseDate(release)} · Firmware verified`;
    ui.flashPanel.hidden = false;
    updateModeUI();
    setLog(`Verified ${release.tag} for ${manifest.board.name}.`);
  } catch (error) {
    if (requestId !== prepareRequestId) return;
    preparedPackage = null;
    ui.releaseNote.textContent = "Firmware could not be verified. Re-select the release or use the CLI instructions.";
    ui.flashPanel.hidden = true;
    setResult("Firmware verification failed", error.message, true);
    setLog(`Firmware error: ${error.message}`);
  }
  setBusy(isBusy);
}

function describeConnectionError(error) {
  if (error?.name === "NotFoundError") return "No serial device was selected. Click Connect device and choose the ESP32-S3 port.";
  if (error?.name === "SecurityError") return "The browser blocked serial access. Open this installer over HTTPS in Chrome or Edge.";
  return "Could not connect to the ESP32-S3. Reconnect the USB-C cable; if needed, use BOOT + RESET download mode and retry.";
}

async function connectDevice() {
  clearResult();
  if (!window.isSecureContext) {
    setConnection("Secure HTTPS is required for USB access.", "error");
    setResult("Open the secure installer", "Web Serial is available only on a secure page. Use the HTTPS GitHub Pages link.", true);
    return;
  }
  if (!("serial" in navigator)) {
    setConnection("This browser does not support USB serial.", "error");
    setResult("Use Chrome or Edge", "This browser cannot access the board’s serial port. Open the installer in Chrome or Edge, or follow the CLI instructions.", true);
    return;
  }

  setBusy(true);
  setConnection("Choose the ESP32-S3 serial device…");
  setLog("Requesting browser serial-port permission.");
  try {
    const port = await navigator.serial.requestPort();
    transport = new Transport(port, false);
    loader = new ESPLoader({
      transport,
      baudrate: 115200,
      debugLogging: false,
      terminal: {
        clean: () => {},
        writeLine: (data) => setLog(data),
        write: (data) => setLog(data),
      },
    });
    const chipName = await loader.main();
    if (!/ESP32-S3/i.test(String(chipName))) {
      throw new Error(`Detected ${chipName || "an unknown chip"}. This installer requires an ESP32-S3.`);
    }
    connectedChip = String(chipName);
    setConnection(`${connectedChip} connected and ready`, "connected");
    ui.flashPanel.hidden = false;
    setLog(`Connected to ${connectedChip}.`);
    clearResult();
  } catch (error) {
    await disconnectDevice();
    setConnection("Device not connected", "error");
    const message = error instanceof Error && error.message.startsWith("Detected ") ? error.message : describeConnectionError(error);
    setResult("Connection needs another try", message, true);
    setLog(`Connection error: ${error?.message ?? error}`);
  }
  setBusy(false);
}

async function loadImages(mode) {
  if (!preparedPackage) throw new Error("Firmware is not ready. Wait for the verification step to finish.");
  const entries = preparedPackage.manifest[mode].images;
  const images = [];
  for (const entry of entries) {
    const file = preparedPackage.zip.file(entry.path);
    if (!file) throw new Error(`Firmware image is missing: ${entry.path}.`);
    const data = await file.async("uint8array");
    const actualHash = await sha256Hex(data);
    if (actualHash.toLowerCase() !== entry.sha256.toLowerCase()) {
      throw new Error(`SHA-256 check failed for ${entry.path}. The device was not flashed.`);
    }
    const address = Number.parseInt(entry.address, 16);
    if (address + data.byteLength > FLASH_LIMIT) throw new Error(`${entry.path} exceeds the board’s flash capacity.`);
    images.push({ data, address, path: entry.path });
  }
  return images;
}

function renderProgress(label, percent) {
  const rounded = Math.max(0, Math.min(100, Math.round(percent)));
  ui.progressWrap.hidden = false;
  ui.progress.value = rounded;
  ui.progressLabel.textContent = label;
  ui.progressPercent.textContent = `${rounded}%`;
}

async function flashFirmware() {
  if (!loader || !connectedChip || !preparedPackage || isBusy) return;
  const mode = getMode();
  if (mode === "fullInstall" && !window.confirm("A new install replaces saved timer settings, theme, and statistics. Wi-Fi and timezone configuration stays on the device. Continue?")) return;

  clearResult();
  setBusy(true);
  ui.flashPanel.hidden = false;
  ui.flashButton.disabled = true;
  const modeName = mode === "fullInstall" ? "full install" : "app update";
  try {
    renderProgress("Verifying firmware images…", 0);
    const images = await loadImages(mode);
    const totalBytes = images.reduce((sum, image) => sum + image.data.byteLength, 0);
    const progressByImage = images.map(() => 0);
    setLog(`Starting ${modeName} for ${preparedPackage.manifest.version} (${connectedChip}).`);
    await loader.writeFlash({
      fileArray: images.map((image) => ({ data: image.data, address: image.address })),
      flashMode: preparedPackage.manifest.flash.mode,
      flashFreq: preparedPackage.manifest.flash.frequency,
      flashSize: preparedPackage.manifest.flash.size,
      eraseAll: false,
      compress: true,
      reportProgress: (fileIndex, written, fileTotal) => {
        progressByImage[fileIndex] = fileTotal > 0 ? Math.max(0, Math.min(written / fileTotal, 1)) : 0;
        const writtenBytes = images.reduce(
          (sum, image, index) => sum + image.data.byteLength * progressByImage[index],
          0,
        );
        const percent = (writtenBytes / totalBytes) * 100;
        const name = images[fileIndex]?.path.split("/").at(-1) ?? "firmware";
        renderProgress(`Writing ${name}…`, percent);
      },
    });
    renderProgress("Flashing complete. Restarting your timer…", 100);
    setLog("All firmware images were written successfully. Resetting the board.");
    try {
      await loader.after("hard_reset");
    } catch (resetError) {
      setLog(`Automatic reset was not confirmed: ${resetError.message}`);
    }
    await disconnectDevice();
    setConnection("Flashing complete", "connected");
    setResult("Tomato32 is installed!", "Your device is restarting. If it appears frozen, unplug the USB-C cable, then press RESET.");
  } catch (error) {
    await disconnectDevice();
    setLog(`Flash error: ${error?.message ?? error}`);
    setConnection("Flash did not finish", "error");
    setResult("Flashing needs another try", `${error?.message ?? "An unexpected error occurred."} Keep the cable connected, return the board to download mode, then connect and retry.`, true);
  }
  setBusy(false);
  ui.flashButton.disabled = !loader || !preparedPackage;
}

async function disconnectDevice() {
  const currentTransport = transport;
  loader = null;
  transport = null;
  connectedChip = null;
  if (currentTransport) {
    try { await currentTransport.disconnect(); } catch { /* The device may already have reset or disconnected. */ }
  }
}

function checkBrowser() {
  const browserSupported = window.isSecureContext && "serial" in navigator;
  if (!window.isSecureContext) {
    setBrowserStatus("Open over HTTPS", "The Web Serial API requires a secure connection.", false);
  } else if (!("serial" in navigator)) {
    setBrowserStatus("Web Serial API unavailable", "Use Chrome or Edge to connect to the device.", false);
  } else {
    setBrowserStatus("Web Serial API available");
  }
  ui.connectButton.disabled = !browserSupported;
}

ui.releaseSelect.addEventListener("change", updateReleaseMetadata);
ui.modeRadios.forEach((radio) => radio.addEventListener("change", updateModeUI));
ui.connectionHelpButton.addEventListener("click", toggleConnectionHelp);
ui.connectButton.addEventListener("click", connectDevice);
ui.flashButton.addEventListener("click", flashFirmware);
navigator.serial?.addEventListener("disconnect", () => {
  if (loader) {
    void disconnectDevice();
    setConnection("USB device disconnected", "error");
    if (isBusy) setResult("Device disconnected", "The flash may be incomplete. Reconnect the board in download mode and retry.", true);
    setBusy(false);
  }
});

checkBrowser();
updateModeUI();
void loadReleaseList();
