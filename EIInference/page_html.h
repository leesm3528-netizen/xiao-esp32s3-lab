#pragma once
// Inference dashboard. Kept out of the .ino (Arduino preprocessor breaks JS there).
static const char PAGE_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="ko"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>XIAO 분류 대시보드</title>
<style>
:root{--bg:#f3f4f6;--card:#fff;--fg:#1f2430;--mute:#6b7280;--line:#e5e7eb;--track:#eceef2;
--c0:#0d9488;--c1:#2563eb;--warn:#d97706;--ok:#16a34a;--bad:#dc2626}
@media (prefers-color-scheme:dark){:root{--bg:#101217;--card:#1a1d24;--fg:#e7e9ee;--mute:#9aa1ad;--line:#2a2e37;--track:#262a33;
--c0:#2dd4bf;--c1:#60a5fa}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif}
header{display:flex;flex-wrap:wrap;gap:8px 14px;align-items:center;padding:14px 16px;max-width:1100px;margin:0 auto}
header h1{font-size:17px;margin:0;flex:1 1 auto}
.pill{display:inline-flex;align-items:center;gap:6px;padding:3px 10px;border-radius:999px;background:var(--card);border:1px solid var(--line);font-size:13px}
.dot{width:8px;height:8px;border-radius:50%;background:var(--mute)}
.dot.on{background:var(--ok)}.dot.off{background:var(--bad)}.dot.pause{background:var(--warn)}
main{max-width:1100px;margin:0 auto;padding:0 16px 24px;display:grid;gap:14px;grid-template-columns:repeat(12,1fr)}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px;min-width:0}
.card h2{font-size:13px;font-weight:600;color:var(--mute);margin:0 0 10px;letter-spacing:.02em}
.s7{grid-column:span 7}.s5{grid-column:span 5}.s4{grid-column:span 4}.s8{grid-column:span 8}.s12{grid-column:span 12}
@media (max-width:860px){.s7,.s5,.s4,.s8{grid-column:span 12}}
.views{display:grid;grid-template-columns:2fr 1fr;gap:12px;align-items:start}
@media (max-width:520px){.views{grid-template-columns:1fr}}
.frame{position:relative;width:100%;aspect-ratio:1;background:#000;border-radius:8px;overflow:hidden}
.frame img{width:100%;height:100%;object-fit:contain;display:block}
#input{image-rendering:pixelated}
.tag{position:absolute;left:8px;top:8px;background:rgba(0,0,0,.6);color:#fff;font-size:12px;padding:2px 8px;border-radius:6px}
.cap{font-size:12px;color:var(--mute);margin:6px 0 0}
#label{font-size:40px;font-weight:700;line-height:1.1;margin:0}
#score{font-size:15px;color:var(--mute);margin:2px 0 12px}
.bar{display:grid;grid-template-columns:96px 1fr 48px;gap:8px;align-items:center;margin:8px 0}
.track{height:12px;background:var(--track);border-radius:6px;overflow:hidden}
.fill{height:100%;width:0;border-radius:6px;transition:width .25s}
.num{text-align:right;font-variant-numeric:tabular-nums}
.led{display:flex;align-items:center;gap:8px;margin-top:12px;padding-top:12px;border-top:1px solid var(--line)}
.bulb{width:14px;height:14px;border-radius:50%;background:var(--track);border:1px solid var(--line)}
.bulb.on{background:#facc15;box-shadow:0 0 10px #facc15}
.pipe{display:flex;flex-wrap:wrap;align-items:stretch;gap:6px}
.step{flex:1 1 120px;border:1px solid var(--line);border-radius:8px;padding:8px 10px}
.step b{display:block;font-size:15px}
.step span{font-size:12px;color:var(--mute)}
.arrow{align-self:center;color:var(--mute)}
.kv{display:grid;grid-template-columns:auto 1fr;gap:4px 12px;margin:0}
.kv dt{color:var(--mute)}.kv dd{margin:0;text-align:right;font-variant-numeric:tabular-nums}
.strip{display:flex;gap:3px;height:28px;margin-bottom:10px}
.strip i{flex:1;border-radius:3px;background:var(--track)}
#hist{list-style:none;margin:0;padding:0;max-height:220px;overflow:auto;font-variant-numeric:tabular-nums}
#hist li{display:grid;grid-template-columns:60px 1fr 52px 64px;gap:8px;padding:4px 2px;border-bottom:1px solid var(--line)}
#hist li span:last-child,#hist li span:nth-child(3){text-align:right;color:var(--mute)}
.sw{display:inline-block;width:10px;height:10px;border-radius:2px;margin-right:6px;vertical-align:-1px}
.ctl{display:flex;flex-wrap:wrap;gap:10px 16px;align-items:center}
button,select{font:inherit;padding:6px 12px;border-radius:8px;border:1px solid var(--line);background:var(--card);color:var(--fg);cursor:pointer}
button.primary{background:var(--c1);border-color:var(--c1);color:#fff}
input[type=range]{width:160px;vertical-align:middle}
.note{color:var(--mute);font-size:12px;margin:10px 0 0}
</style></head><body>
<header>
  <h1>XIAO ESP32-S3 · Edge Impulse 분류</h1>
  <span class="pill"><span id="dot" class="dot"></span><span id="state">연결 중</span></span>
  <span class="pill" id="ip">-</span>
</header>
<main>
  <section class="card s7">
    <h2>무엇을 찍고 있나</h2>
    <div class="views">
      <div>
        <div class="frame"><img id="view" alt="카메라 실시간 영상"><span class="tag" id="viewTag">카메라</span></div>
        <p class="cap" id="viewCap">실시간 영상</p>
      </div>
      <div>
        <div class="frame"><img id="input" alt="모델 입력 이미지"><span class="tag">모델 입력</span></div>
        <p class="cap" id="inputCap">모델이 실제로 본 96×96 이미지</p>
      </div>
    </div>
  </section>

  <section class="card s5">
    <h2>판정 결과</h2>
    <p id="label">-</p>
    <p id="score">기다리는 중</p>
    <div id="bars"></div>
    <div class="led"><span id="bulb" class="bulb"></span><span id="ledText">LED -</span></div>
  </section>

  <section class="card s12">
    <h2>어떻게 찍고 판정하나</h2>
    <div class="pipe">
      <div class="step"><b id="pCam">-</b><span>카메라 JPEG</span></div><span class="arrow">→</span>
      <div class="step"><b id="pDec">RGB888</b><span>JPEG 풀기</span></div><span class="arrow">→</span>
      <div class="step"><b id="pIn">96×96</b><span>가운데 자르고 줄이기</span></div><span class="arrow">→</span>
      <div class="step"><b>MobileNetV1 0.25</b><span>int8 전이학습 모델</span></div><span class="arrow">→</span>
      <div class="step"><b id="pTime">-</b><span id="pTimeSub">처리 시간</span></div>
    </div>
  </section>

  <section class="card s8">
    <h2>최근 판정 <span id="histN"></span></h2>
    <div class="strip" id="strip"></div>
    <ul id="hist"></ul>
  </section>

  <section class="card s4">
    <h2>누적 집계</h2>
    <div id="counts"></div>
    <h2 style="margin-top:16px">보드</h2>
    <dl class="kv" id="board"></dl>
  </section>

  <section class="card s12">
    <h2>조작</h2>
    <div class="ctl">
      <button id="pause" class="primary">추론 멈추기</button>
      <button id="stream">영상 끄기</button>
      <label>LED 켤 클래스 <select id="ledLabel"></select></label>
      <label>기준 <input id="thr" type="range" min="0.5" max="0.99" step="0.01"> <b id="thrV">-</b></label>
      <button id="reset">집계 초기화</button>
    </div>
    <p class="note">"background" 클래스가 없어서 빈 화면이나 다른 물건도 둘 중 하나로 판정한다. 학습 사진과 비슷한 거리·배경에서 가장 잘 맞는다.</p>
  </section>
</main>
<script>
const $ = (id) => document.getElementById(id);
const COLORS = ['var(--c0)', 'var(--c1)', '#a855f7', '#f59e0b'];
const streamUrl = 'http://' + location.hostname + ':81/stream';
let labels = [], streaming = true, paused = false, bars = {}, thrDragging = false;

$('view').src = streamUrl;
$('stream').onclick = () => {
  streaming = !streaming;
  $('view').src = streaming ? streamUrl : '';
  $('stream').textContent = streaming ? '영상 끄기' : '영상 켜기';
  $('viewCap').textContent = streaming ? '실시간 영상' : '영상 꺼짐 (추론은 계속)';
};

function color(label) { const i = labels.indexOf(label); return COLORS[(i < 0 ? 0 : i) % COLORS.length]; }

function setupLabels(list) {
  labels = list;
  $('bars').innerHTML = '';
  $('ledLabel').innerHTML = '';
  list.forEach((name) => {
    const d = document.createElement('div');
    d.className = 'bar';
    d.innerHTML = '<span></span><div class="track"><div class="fill"></div></div><span class="num">-</span>';
    d.children[0].textContent = name;
    d.querySelector('.fill').style.background = color(name);
    $('bars').appendChild(d);
    bars[name] = { fill: d.querySelector('.fill'), num: d.querySelector('.num') };
    const o = document.createElement('option');
    o.value = o.textContent = name;
    $('ledLabel').appendChild(o);
  });
}

function fmtUptime(s) {
  const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60);
  return h ? h + '시간 ' + m + '분' : m + '분 ' + (s % 60) + '초';
}

function render(j) {
  if (!labels.length && j.labels) setupLabels(j.labels);
  paused = j.paused;
  $('pause').textContent = paused ? '추론 다시 시작' : '추론 멈추기';
  $('dot').className = 'dot ' + (paused ? 'pause' : 'on');
  $('state').textContent = paused ? '일시정지' : (j.fps ? j.fps.toFixed(1) + '회/초 추론 중' : '추론 중');
  $('ip').textContent = j.board.ip;

  if (j.ok) {
    $('label').textContent = j.label;
    $('label').style.color = color(j.label);
    $('score').textContent = '확신도 ' + Math.round(j.score * 100) + '% · #' + j.count + ' · ' + (j.age_ms / 1000).toFixed(1) + '초 전';
    for (const k in j.scores) {
      if (!bars[k]) continue;
      bars[k].fill.style.width = (j.scores[k] * 100) + '%';
      bars[k].num.textContent = Math.round(j.scores[k] * 100) + '%';
    }
    $('pCam').textContent = j.src.w + '×' + j.src.h;
    $('pIn').textContent = j.input.w + '×' + j.input.h;
    $('pTime').textContent = j.total_ms + ' ms';
    $('pTimeSub').textContent = '전처리 ' + j.dsp_ms + ' · 추론 ' + j.nn_ms + ' ms';
    $('viewTag').textContent = '카메라 ' + j.src.w + '×' + j.src.h + ' · JPEG ' + (j.src.bytes / 1024).toFixed(1) + 'KB';
  } else {
    $('score').textContent = j.camera.ok ? (paused ? '일시정지' : '첫 결과 기다리는 중') : '카메라 없음';
  }

  $('bulb').className = 'bulb' + (j.led ? ' on' : '');
  $('ledText').textContent = 'LED ' + (j.led ? '켜짐' : '꺼짐') + ' — ' + j.led_label + ' ' + Math.round(j.threshold * 100) + '% 이상일 때 켜짐';
  if (document.activeElement !== $('ledLabel')) $('ledLabel').value = j.led_label;
  if (!thrDragging) { $('thr').value = j.threshold; $('thrV').textContent = Math.round(j.threshold * 100) + '%'; }

  let total = 0;
  for (const k in j.counts) total += j.counts[k];
  $('counts').innerHTML = '';
  labels.forEach((k) => {
    const n = j.counts[k] || 0, pct = total ? n / total * 100 : 0;
    const d = document.createElement('div');
    d.className = 'bar';
    d.innerHTML = '<span></span><div class="track"><div class="fill"></div></div><span class="num"></span>';
    d.children[0].textContent = k;
    d.querySelector('.fill').style.cssText = 'width:' + pct + '%;background:' + color(k);
    d.querySelector('.num').textContent = n;
    $('counts').appendChild(d);
  });

  const b = j.board, c = j.camera;
  $('board').innerHTML = '';
  [['카메라', c.ok ? '정상 · 품질 ' + c.quality : '없음'], ['영상 시청', c.viewers + ' / 3'],
   ['Wi-Fi', b.rssi + ' dBm'], ['메모리', (b.heap / 1024).toFixed(0) + ' KB'],
   ['PSRAM', (b.psram / 1048576).toFixed(1) + ' MB'], ['켜진 시간', fmtUptime(b.uptime)]].forEach(([k, v]) => {
    const dt = document.createElement('dt'), dd = document.createElement('dd');
    dt.textContent = k; dd.textContent = v;
    $('board').append(dt, dd);
  });
}

function renderHistory(items) {
  $('histN').textContent = '(' + items.length + '개)';
  $('strip').innerHTML = '';
  items.slice().reverse().forEach((it) => {
    const i = document.createElement('i');
    i.style.background = color(it.label);
    i.style.opacity = 0.35 + it.score * 0.65;
    i.title = it.label + ' ' + Math.round(it.score * 100) + '%';
    $('strip').appendChild(i);
  });
  $('hist').innerHTML = '';
  items.forEach((it) => {
    const li = document.createElement('li');
    li.innerHTML = '<span></span><span><i class="sw"></i></span><span></span><span></span>';
    li.children[0].textContent = '#' + it.n;
    li.children[1].querySelector('.sw').style.background = color(it.label);
    li.children[1].append(it.label);
    li.children[2].textContent = Math.round(it.score * 100) + '%';
    li.children[3].textContent = (it.ago_ms / 1000).toFixed(1) + '초 전';
    $('hist').appendChild(li);
  });
}

async function api(path) {
  const r = await fetch(path, { cache: 'no-store' });
  return r.json();
}

let fails = 0;
async function poll() {
  try {
    render(await api('/api/result'));
    fails = 0;
  } catch (e) {
    if (++fails > 2) { $('dot').className = 'dot off'; $('state').textContent = '보드 응답 없음'; }
  }
  setTimeout(poll, 400);
}

async function pollHistory() {
  try { renderHistory((await api('/api/history')).items); } catch (e) {}
  setTimeout(pollHistory, 1000);
}

function pollInput() {
  if (paused) { setTimeout(pollInput, 1000); return; }
  const img = new Image();
  img.onload = () => { $('input').src = img.src; setTimeout(pollInput, 500); };
  img.onerror = () => setTimeout(pollInput, 1500);
  img.src = '/api/input.bmp?t=' + Date.now();
}

$('pause').onclick = async () => render(await api('/api/config?paused=' + (paused ? 0 : 1)));
$('reset').onclick = async () => { render(await api('/api/reset')); renderHistory([]); };
$('ledLabel').onchange = async (e) => render(await api('/api/config?label=' + encodeURIComponent(e.target.value)));
$('thr').oninput = (e) => { thrDragging = true; $('thrV').textContent = Math.round(e.target.value * 100) + '%'; };
$('thr').onchange = async (e) => { thrDragging = false; render(await api('/api/config?threshold=' + e.target.value)); };

poll();
pollHistory();
pollInput();
</script></body></html>)rawliteral";
