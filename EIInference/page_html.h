#pragma once
// Live view + classification result. Kept out of the .ino (Arduino preprocessor breaks JS there).
static const char PAGE_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="ko"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>XIAO 분류</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--fg:#1d2330;--mute:#6b7280;--bar:#e5e7eb;--a:#2563eb;--b:#16a34a}
@media (prefers-color-scheme:dark){:root{--bg:#111318;--card:#1b1e25;--fg:#e8eaf0;--mute:#9aa1ad;--bar:#2b2f38}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 system-ui,sans-serif}
main{max-width:760px;margin:0 auto;padding:16px;display:grid;gap:14px}
.card{background:var(--card);border-radius:12px;padding:14px}
h1{font-size:18px;margin:0}
#view{width:100%;max-width:480px;aspect-ratio:1;display:block;margin:0 auto;background:#000;border-radius:8px;object-fit:contain}
#label{font-size:34px;font-weight:700;margin:0}
#sub{color:var(--mute);font-size:13px}
.row{display:grid;grid-template-columns:110px 1fr 54px;gap:8px;align-items:center;margin:6px 0}
.track{height:14px;background:var(--bar);border-radius:7px;overflow:hidden}
.fill{height:100%;width:0;background:var(--a);transition:width .2s}
.row:nth-child(2) .fill{background:var(--b)}
.num{text-align:right;font-variant-numeric:tabular-nums}
button{font:inherit;padding:6px 12px;border-radius:8px;border:1px solid var(--bar);background:var(--card);color:var(--fg)}
.note{color:var(--mute);font-size:13px;margin:0}
</style></head><body><main>
<div class="card"><h1>XIAO ESP32-S3 · Edge Impulse 분류</h1>
<p class="note">모델 입력 96×96 · 카메라 240×240 · 결과는 0.2초마다 갱신</p></div>
<div class="card">
<img id="view" alt="카메라">
<p style="text-align:center;margin:8px 0 0"><button id="toggle">영상 끄기</button></p>
</div>
<div class="card">
<p id="label">-</p><p id="sub">연결 중…</p>
<div id="bars"></div>
<p class="note">"background" 클래스가 없어서 빈 화면도 둘 중 하나로 판정한다.</p>
</div>
</main>
<script>
const view = document.getElementById('view');
const streamUrl = 'http://' + location.hostname + ':81/stream';
let streaming = true;
view.src = streamUrl;
document.getElementById('toggle').onclick = function () {
  streaming = !streaming;
  view.src = streaming ? streamUrl : '';
  this.textContent = streaming ? '영상 끄기' : '영상 켜기';
};
const bars = document.getElementById('bars');
const rows = {};
function row(name) {
  if (rows[name]) return rows[name];
  const d = document.createElement('div');
  d.className = 'row';
  d.innerHTML = '<span></span><div class="track"><div class="fill"></div></div><span class="num"></span>';
  d.children[0].textContent = name;
  bars.appendChild(d);
  rows[name] = { fill: d.querySelector('.fill'), num: d.querySelector('.num') };
  return rows[name];
}
async function poll() {
  try {
    const r = await fetch('/api/result', { cache: 'no-store' });
    const j = await r.json();
    if (j.ok) {
      document.getElementById('label').textContent = j.label + ' ' + Math.round(j.score * 100) + '%';
      document.getElementById('sub').textContent = '#' + j.count + ' · DSP ' + j.dsp_ms + ' ms · 추론 ' + j.nn_ms +
        ' ms · 전체 ' + j.total_ms + ' ms · LED ' + (j.led ? '켜짐' : '꺼짐') + ' (' + j.led_label + ' ≥ ' + j.threshold + ')';
      for (const k in j.scores) {
        const e = row(k);
        e.fill.style.width = (j.scores[k] * 100) + '%';
        e.num.textContent = Math.round(j.scores[k] * 100) + '%';
      }
    } else {
      document.getElementById('sub').textContent = j.camera ? '첫 결과 기다리는 중' : '카메라 없음';
    }
  } catch (e) {
    document.getElementById('sub').textContent = '보드 응답 없음';
  }
  setTimeout(poll, 300);
}
poll();
</script></body></html>)rawliteral";
