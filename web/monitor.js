"use strict";

const SERVICE = "12340000-1234-5678-9abc-def012345678";
const STREAM = "12340001-1234-5678-9abc-def012345678";
const HZ = 400;
const MV_PER_COUNT = (2 * 2400) / (3.5 * 0x800000);
const WINDOW = 5 * HZ;
const names = ["I", "II", "V1", "III", "aVR", "aVL", "aVF"];
const $ = id => document.getElementById(id);
const traces = names.map((name, i) => {
    const row = document.createElement("article");
    row.innerHTML = '<div class="channel"><strong></strong><output>— mV</output></div><canvas></canvas>';
    row.querySelector("strong").textContent = "Lead " + name + (i >= 3 ? " · derived" : "");
    row.querySelector("canvas").setAttribute("aria-label", "Lead " + name + " waveform");
    $("channels").appendChild(row);
    return {row, canvas: row.querySelector("canvas"), value: row.querySelector("output")};
});

let device, characteristic, streaming = false, connecting = false;
let windowSamples = [], expected = null, position = -1, missing = 0, received = 0;
let lastArrival = 0, rateStart = performance.now(), lastPaint = 0, dirty = true;

function signed24(view, k) {
    const raw = view.getUint8(k) | (view.getUint8(k+1) << 8) | (view.getUint8(k+2) << 16);
    return raw & 0x800000 ? raw - 0x1000000 : raw;
}

function onSample(event) {
    const v = event.target.value;
    if (v.byteLength !== 11 && v.byteLength !== 20) {
        $("status").textContent = "Unexpected packet size · check firmware version";
        return;
    }
    const first = v.getUint16(0, true);
    const gap = expected === null ? 0 : (first - expected + 65536) % 65536;
    position += gap;
    missing += gap;
    for (let k = 2; k < v.byteLength; k += 9) {
        const i = signed24(v,k) * MV_PER_COUNT;
        const ii = signed24(v,k+3) * MV_PER_COUNT;
        const v1 = signed24(v,k+6) * MV_PER_COUNT;
        windowSamples.push({x: ++position, y:[i, ii, v1, ii-i, -(i+ii)/2, i-ii/2, ii-i/2]});
        received++;
    }
    expected = (first + (v.byteLength-2)/9) & 65535;
    while (windowSamples.length && windowSamples[0].x <= position - WINDOW) windowSamples.shift();
    lastArrival = performance.now();
    dirty = true;
}

function resetWindow() {
    windowSamples = [];
    expected = null;
    position = -1;
    received = 0;
    missing = 0;
    lastArrival = 0;
    rateStart = performance.now();
    $("rate").textContent = "0 samples/s received";
    $("drops").textContent = "0 missing sample indices";
    dirty = true;
}

function cleanup() {
    if (characteristic) characteristic.removeEventListener("characteristicvaluechanged", onSample);
    if (device) device.removeEventListener("gattserverdisconnected", disconnected);
    characteristic = null;
    device = null;
    streaming = false;
    $("connect").textContent = "Connect ECG";
    $("connect").disabled = false;
    $("rate").textContent = "0 samples/s received";
}

function disconnected() {
    cleanup();
    $("status").textContent = "Disconnected · capture paused";
}

$("connect").onclick = async () => {
    if (connecting) return;
    if (device?.gatt.connected) { device.gatt.disconnect(); return; }
    if (!navigator.bluetooth || !window.isSecureContext) {
        $("status").textContent = "Open this page on localhost in Chrome or Edge with Bluetooth enabled.";
        return;
    }
    connecting = true;
    $("connect").disabled = true;
    try {
        device = await navigator.bluetooth.requestDevice({
            filters: [{name: "STM32-ECG"}], optionalServices: [SERVICE]
        });
        device.addEventListener("gattserverdisconnected", disconnected);
        $("status").textContent = "Connecting…";
        const gatt = await device.gatt.connect();
        const service = await gatt.getPrimaryService(SERVICE);
        characteristic = await service.getCharacteristic(STREAM);
        resetWindow();
        characteristic.addEventListener("characteristicvaluechanged", onSample);
        await characteristic.startNotifications();
        streaming = true;
        $("connect").textContent = "Disconnect";
        $("status").textContent = "Subscribed · waiting for ECG";
    } catch (err) {
        const old = device;
        cleanup();
        if (old?.gatt.connected) old.gatt.disconnect();
        $("status").textContent = err.name === "NotFoundError" ? "No device selected or found" : err.message;
    } finally {
        connecting = false;
        $("connect").disabled = false;
    }
};

function draw(trace, ch) {
    const {canvas, value} = trace;
    const w = canvas.clientWidth, h = canvas.clientHeight;
    if (w <= 76) return;
    const dpr = window.devicePixelRatio || 1;
    if (canvas.width !== Math.round(w*dpr) || canvas.height !== Math.round(h*dpr)) {
        canvas.width = Math.round(w*dpr);
        canvas.height = Math.round(h*dpr);
    }
    const ctx = canvas.getContext("2d");
    ctx.setTransform(dpr,0,0,dpr,0,0);
    ctx.clearRect(0,0,w,h);
    const left = 70, top = 12, bottom = h-12;
    let lo = Infinity, hi = -Infinity;
    for (const s of windowSamples) { lo = Math.min(lo,s.y[ch]); hi = Math.max(hi,s.y[ch]); }
    const mid = windowSamples.length ? (lo+hi)/2 : 0;
    const span = windowSamples.length ? Math.max(0.4,(hi-lo)*1.16) : 2;
    ctx.font = "11px monospace";
    ctx.fillStyle = "#9bb5ac";
    ctx.strokeStyle = "#2c4740";
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (let k=0; k<=5; k++) {
        const x = left + k*(w-left-1)/5;
        ctx.moveTo(x,top); ctx.lineTo(x,bottom);
    }
    for (let k=0; k<=2; k++) {
        const y = top+k*(bottom-top)/2;
        ctx.fillText((mid+span*(0.5-k/2)).toFixed(2),2,y+4);
        ctx.moveTo(left,y); ctx.lineTo(w,y);
    }
    ctx.stroke();
    ctx.strokeStyle = ch < 3 ? "#8eebbd" : "#82c1f0";
    ctx.lineWidth = 1.6;
    ctx.beginPath();
    let previous = null;
    for (const s of windowSamples) {
        const x = w-1-(position-s.x)*(w-left-1)/WINDOW;
        const y = top+(0.5-(s.y[ch]-mid)/span)*(bottom-top);
        if (previous === null || s.x !== previous+1) ctx.moveTo(x,y);
        else ctx.lineTo(x,y);
        previous = s.x;
    }
    ctx.stroke();
    value.textContent = windowSamples.length ? windowSamples[windowSamples.length-1].y[ch].toFixed(3)+" mV" : "— mV";
}

function updateRows() {
    traces.forEach((t,i) => { t.row.hidden = i >= Number($("leads").value); });
    dirty = true;
}
$("leads").onchange = updateRows;
window.addEventListener("resize", () => { dirty = true; });
updateRows();

function frame(now) {
    if (streaming && now-rateStart >= 1000) {
        $("rate").textContent = Math.round(received*1000/(now-rateStart))+" samples/s received";
        $("drops").textContent = missing+" missing sample indices";
        $("status").textContent = !lastArrival ? "Subscribed · waiting for ECG" :
            (now-lastArrival > 1200 ? "Subscribed · data stalled" : "Streaming");
        received = 0;
        rateStart = now;
    }
    if (dirty && now-lastPaint >= 50) {
        traces.forEach((trace,i) => { if (!trace.row.hidden) draw(trace,i); });
        lastPaint = now;
        dirty = false;
    }
    requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
