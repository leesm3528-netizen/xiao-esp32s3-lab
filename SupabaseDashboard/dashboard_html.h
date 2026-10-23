// Local dashboard served at http://<ip>/ (LED, touch, camera)
// kept out of the .ino: the Arduino preprocessor injects prototypes into JS functions inside raw strings
#pragma once

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>XIAO 대시보드</title>
<style>
  :root { --bg:#f4f5f7; --card:#fff; --text:#1d1f23; --muted:#6b7280; --on:#f59e0b; --touch:#10b981; --off:#d1d5db; --err:#ef4444; }
  @media (prefers-color-scheme: dark) {
    :root { --bg:#111317; --card:#1b1e24; --text:#e8eaed; --muted:#9aa0a6; --off:#3a3f47; }
  }
  * { box-sizing:border-box; }
  body { margin:0; font-family:system-ui,-apple-system,"Malgun Gothic",sans-serif; background:var(--bg); color:var(--text); }
  main { max-width:720px; margin:0 auto; padding:24px 16px; }
  h1 { font-size:20px; margin:0 0 4px; }
  .sub { color:var(--muted); font-size:13px; margin-bottom:20px; }
  .sub.err { color:var(--err); }
  .grid { display:grid; grid-template-columns:1fr 1fr; gap:16px; }
  @media (max-width:560px) { .grid { grid-template-columns:1fr; } }
  .card { background:var(--card); border-radius:14px; padding:20px; box-shadow:0 1px 3px rgba(0,0,0,.08); }
  .cam { margin-top:16px; padding:16px; }
  .row { display:flex; align-items:center; justify-content:space-between; gap:12px; }
  .label { font-size:15px; font-weight:600; }
  .state { font-size:13px; color:var(--muted); margin-top:4px; }
  .dot { width:48px; height:48px; flex:none; border-radius:50%; background:var(--off); transition:background .15s, box-shadow .15s; }
  .dot.led { background:var(--on); box-shadow:0 0 20px var(--on); }
  .dot.touch { background:var(--touch); box-shadow:0 0 20px var(--touch); }
  .btns { display:flex; gap:10px; margin-top:16px; flex-wrap:wrap; }
  button, select, a.btn { flex:1; min-width:110px; padding:12px; font-size:15px; font-weight:600; border:0; border-radius:10px; cursor:pointer;
    background:var(--off); color:var(--text); text-align:center; text-decoration:none; font-family:inherit; }
  button.primary { background:var(--on); color:#fff; }
  .meta { font-size:12px; color:var(--muted); margin-top:12px; font-variant-numeric:tabular-nums; }
  .view { background:#000; border-radius:10px; overflow:hidden; aspect-ratio:4/3; margin-top:12px; display:flex; align-items:center; justify-content:center; color:#9aa0a6; font-size:14px; }
  .view img { width:100%; height:100%; object-fit:contain; display:block; }
</style>
</head>
<body>
<main>
  <h1>XIAO ESP32-S3 대시보드</h1>
  <div class="sub" id="conn">연결 중…</div>

  <div class="grid">
    <div class="card">
      <div class="row">
        <div><div class="label">LED (GPIO21)</div><div class="state" id="ledState">-</div></div>
        <div class="dot" id="ledDot"></div>
      </div>
      <div class="btns">
        <button class="primary" onclick="setLed('on')">켜기</button>
        <button onclick="setLed('off')">끄기</button>
      </div>
    </div>
    <div class="card">
      <div class="row">
        <div><div class="label">터치센서 (D0)</div><div class="state" id="touchState">-</div></div>
        <div class="dot" id="touchDot"></div>
      </div>
      <div class="meta" id="touchMeta">-</div>
    </div>
  </div>

  <div class="card cam">
    <div class="row">
      <div><div class="label">카메라</div><div class="state" id="camState">-</div></div>
    </div>
    <div class="view" id="view"><img id="stream" alt="카메라 영상"></div>
    <div class="btns">
      <button class="primary" id="toggle" onclick="toggleStream()">일시정지</button>
      <select id="size" onchange="setSize(this.value)" aria-label="해상도">
        <option value="5">QVGA 320×240</option>
        <option value="8" selected>VGA 640×480</option>
        <option value="9">SVGA 800×600</option>
        <option value="11">HD 1280×720</option>
      </select>
      <a class="btn" id="captureLink" href="#" target="_blank">사진 저장</a>
    </div>
  </div>
</main>
<script>
// https (Tailscale Serve) -> origin + '/cam'; http (same Wi-Fi) -> port 81
const CAM_BASE = location.protocol === 'https:' ? location.origin + '/cam' : 'http://' + location.hostname + ':81';
const img = document.getElementById('stream');
let playing = false;
let retryTimer = null;

document.getElementById('captureLink').href = CAM_BASE + '/capture';

function render(s) {
  document.getElementById('ledState').textContent = s.led ? '켜짐' : '꺼짐';
  document.getElementById('ledDot').className = 'dot' + (s.led ? ' led' : '');
  document.getElementById('touchState').textContent = s.touched ? '터치 중' : '터치 안 함';
  document.getElementById('touchDot').className = 'dot' + (s.touched ? ' touch' : '');
  document.getElementById('touchMeta').textContent = '값 ' + s.value + ' / 기준 ' + s.baseline;
  document.getElementById('camState').textContent = s.camera ? '시청 ' + s.viewers + '/3' : '카메라 없음';
  const c = document.getElementById('conn');
  c.textContent = '연결됨 · ' + location.host + ' · 클라우드 ' + (s.cloud ? '동기화 중' : '끊김');
  c.className = 'sub' + (s.cloud ? '' : ' err');
}
function fail() {
  const c = document.getElementById('conn');
  c.textContent = '보드와 연결 끊김'; c.className = 'sub err';
}
async function poll() {
  try { render(await (await fetch('/api/status')).json()); } catch (e) { fail(); }
  setTimeout(poll, 300);
}
async function setLed(state) {
  try { render(await (await fetch('/api/led?state=' + state, { method: 'POST' })).json()); } catch (e) { fail(); }
}

function startStream() {
  clearTimeout(retryTimer);
  img.src = CAM_BASE + '/stream?t=' + Date.now();
  playing = true;
  document.getElementById('toggle').textContent = '일시정지';
}
function stopStream() {
  clearTimeout(retryTimer);
  img.removeAttribute('src');
  playing = false;
  document.getElementById('toggle').textContent = '재생';
}
function toggleStream() { playing ? stopStream() : startStream(); }
img.onerror = () => {
  if (!playing) return;
  document.getElementById('camState').textContent = '영상 끊김 · 시청자가 3명이면 자리가 날 때까지 기다립니다. 3초 후 다시 연결';
  retryTimer = setTimeout(() => { if (playing) startStream(); }, 3000);
};
async function setSize(v) {
  stopStream();
  try { await fetch('/api/camera?size=' + v, { method: 'POST' }); } catch (e) { fail(); }
  startStream();
}

poll();
startStream();
</script>
</body>
</html>)rawliteral";
