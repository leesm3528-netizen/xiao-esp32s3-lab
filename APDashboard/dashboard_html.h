// Dashboard served at http://192.168.4.1/ (LED, touch)
// kept out of the .ino: the Arduino preprocessor injects prototypes into JS functions inside raw strings
#pragma once

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>XIAO AP 대시보드</title>
<style>
  :root { --bg:#f4f5f7; --card:#fff; --text:#1d1f23; --muted:#6b7280; --on:#f59e0b; --touch:#10b981; --off:#d1d5db; --err:#ef4444; }
  @media (prefers-color-scheme: dark) {
    :root { --bg:#111317; --card:#1b1e24; --text:#e8eaed; --muted:#9aa0a6; --off:#3a3f47; }
  }
  * { box-sizing:border-box; }
  body { margin:0; font-family:system-ui,-apple-system,"Malgun Gothic",sans-serif; background:var(--bg); color:var(--text); }
  main { max-width:480px; margin:0 auto; padding:24px 16px; }
  h1 { font-size:20px; margin:0 0 4px; }
  .sub { color:var(--muted); font-size:13px; margin-bottom:20px; }
  .sub.err { color:var(--err); }
  .card { background:var(--card); border-radius:14px; padding:20px; margin-bottom:16px; box-shadow:0 1px 3px rgba(0,0,0,.08); }
  .row { display:flex; align-items:center; justify-content:space-between; gap:12px; }
  .label { font-size:15px; font-weight:600; }
  .state { font-size:13px; color:var(--muted); margin-top:4px; }
  .dot { width:56px; height:56px; flex:none; border-radius:50%; background:var(--off); transition:background .15s, box-shadow .15s; }
  .dot.led { background:var(--on); box-shadow:0 0 24px var(--on); }
  .dot.touch { background:var(--touch); box-shadow:0 0 24px var(--touch); }
  .btns { display:flex; gap:10px; margin-top:16px; }
  button { flex:1; padding:14px; font-size:16px; font-weight:600; border:0; border-radius:10px; cursor:pointer; background:var(--off); color:var(--text); font-family:inherit; }
  button.primary { background:var(--on); color:#fff; }
  .value { font-size:32px; font-weight:700; font-variant-numeric:tabular-nums; margin-top:12px; }
  .bar { height:8px; border-radius:4px; background:var(--off); margin-top:10px; overflow:hidden; }
  .bar > div { height:100%; width:0; background:var(--touch); transition:width .15s; }
  .meta { font-size:12px; color:var(--muted); margin-top:8px; font-variant-numeric:tabular-nums; }
</style>
</head>
<body>
<main>
  <h1>XIAO ESP32-S3</h1>
  <div class="sub" id="conn">연결 중…</div>

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
    <div class="value" id="touchValue">-</div>
    <div class="bar"><div id="touchBar"></div></div>
    <div class="meta" id="touchMeta">-</div>
  </div>
</main>
<script>
function render(s) {
  document.getElementById('ledState').textContent = s.led ? '켜짐' : '꺼짐';
  document.getElementById('ledDot').className = 'dot' + (s.led ? ' led' : '');
  document.getElementById('touchState').textContent = s.touched ? '터치 중' : '터치 안 함';
  document.getElementById('touchDot').className = 'dot' + (s.touched ? ' touch' : '');
  document.getElementById('touchValue').textContent = s.value.toLocaleString();
  // bar: baseline = 0%, threshold (baseline x1.3) = 100%
  const pct = Math.max(0, Math.min(100, (s.value - s.baseline) / (s.baseline * 0.3) * 100));
  document.getElementById('touchBar').style.width = pct + '%';
  document.getElementById('touchMeta').textContent = '기준 ' + s.baseline.toLocaleString() + ' · 터치 판정 ' + Math.round(s.baseline * 1.3).toLocaleString() + ' 이상';
  const c = document.getElementById('conn');
  c.textContent = '연결됨 · ' + location.host + ' · 접속 기기 ' + s.clients + '대';
  c.className = 'sub';
}
function fail() {
  const c = document.getElementById('conn');
  c.textContent = '보드와 연결 끊김 · 휴대폰이 보드 Wi-Fi에 연결되어 있는지 확인하세요';
  c.className = 'sub err';
}
async function poll() {
  try { render(await (await fetch('/api/status')).json()); } catch (e) { fail(); }
  setTimeout(poll, 200);
}
async function setLed(state) {
  try { render(await (await fetch('/api/led?state=' + state, { method: 'POST' })).json()); } catch (e) { fail(); }
}
poll();
</script>
</body>
</html>)rawliteral";
