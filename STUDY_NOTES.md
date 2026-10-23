# XIAO ESP32-S3 학습 정리

[README](README.md)의 키워드를 코드와 함께 조금 더 자세히 정리했습니다.

## 목차

1. [LED와 터치센서](#1-led와-터치센서)
2. [같은 Wi-Fi 웹서버 (STA 모드)](#2-같은-wi-fi-웹서버-sta-모드)
3. [밖에서 제어하기: Supabase](#3-밖에서-제어하기-supabase)
4. [카메라 스트리밍](#4-카메라-스트리밍)
5. [AP 모드](#5-ap-모드)
6. [BLE](#6-ble)
7. [연결 방식 비교](#7-연결-방식-비교)
8. [AI 에이전트와 작업하며 배운 것](#8-ai-에이전트와-작업하며-배운-것)

---

## 1. LED와 터치센서

### active-low LED

XIAO의 유저 LED(GPIO21)는 핀이 `LOW`일 때 켜집니다. 그래서 켜고 끄는 코드를 함수 하나로 감싸 두고 어디서나 그 함수만 씁니다.

```cpp
void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, on ? LOW : HIGH);  // LOW = ON
}
```

### 정전식 터치와 기준값

`touchRead(1)`은 D0 핀의 정전 용량을 숫자로 돌려줍니다. 원조 ESP32는 손을 대면 값이 내려가지만 **ESP32-S3는 올라갑니다**. 평소 값은 약 17,000이었습니다.

값은 습도나 손 위치에 따라 조금씩 흔들리므로 고정된 숫자 대신 **기준값 대비 비율**로 판정합니다.

```cpp
touchValue = touchRead(TOUCH_PIN);
touched = touchValue > baseline * 1.3;        // 기준값보다 30% 높으면 터치
if (!touched) {
  baseline = (baseline * 15 + touchValue) / 16;  // 터치 안 할 때만 기준값을 천천히 따라감
}
```

부팅 때 20번 읽은 평균이 첫 기준값이라, **부팅 중에는 D0을 만지면 안 됩니다.**

---

## 2. 같은 Wi-Fi 웹서버 (STA 모드)

보드가 집 공유기에 접속하고(STA), 80번 포트에 웹서버를 엽니다. 페이지의 JS가 0.2초마다 `/api/status`를 불러 화면을 갱신하고, 버튼은 `/api/led?state=on`을 호출합니다.

| 주소 | 하는 일 |
|---|---|
| `/` | 대시보드 HTML |
| `/api/status` | `{"led":false,"touched":false,"value":16940,"baseline":16934}` |
| `/api/led?state=on\|off\|toggle` | LED를 바꾸고 새 상태를 돌려줌 |

`ESPmDNS`로 `xiao.local`이라는 이름도 붙였습니다. IP가 바뀌어도 이름으로 접속할 수 있습니다(안드로이드 일부 기기는 mDNS 미지원).

**한계:** 보드 IP(`192.168.0.x`)는 공유기 안에서만 통하는 사설 주소라 집 밖에서는 접속할 수 없습니다.

---

## 3. 밖에서 제어하기: Supabase

### 구조

보드와 휴대폰이 서로 직접 연결하는 대신, **둘 다 인터넷 위의 DB(Supabase)에 접속**해서 값을 주고받습니다.

```
휴대폰 (어디서나)  ──HTTPS──►  Supabase (xiao_state 테이블)  ◄──HTTPS──  보드 (집 Wi-Fi)
   LED 켜기 = led_on 변경          실시간으로 페이지에 알림            0.2초마다 보고 + LED 값 받기
```

### 권한 설계

공개 키(publishable key)는 웹페이지에 들어가므로 누구나 볼 수 있습니다. 그래서 키 자체가 아니라 **DB 권한**으로 막아야 합니다.

| 누가 | 무엇을 할 수 있나 | 어떻게 막았나 |
|---|---|---|
| 웹페이지 (공개 키) | 상태 읽기, `led_on` 바꾸기 | RLS 정책 + `grant update (led_on)` 열 단위 권한 |
| 웹페이지 | 터치값 조작, 행 추가·삭제 | 권한 없음 → `permission denied` |
| 보드 | 터치값·IP 저장, LED 읽기 | `xiao_report()` 함수. **장치 시크릿**이 맞을 때만 실행 |

보드가 부르는 함수는 하나뿐이고, 응답이 곧 LED 목표값이라 요청 한 번으로 보고와 명령 받기를 같이 합니다.

```cpp
// POST /rest/v1/rpc/xiao_report  ->  "true" / "false"
int code = http.POST(body);
setLed(http.getString() == "true");
```

### 실시간 반영과 온라인 표시

- 웹페이지는 Supabase **Realtime**을 구독해서 값이 바뀌는 즉시 화면을 갱신합니다.
- 보드는 2초마다 `device_seen_at`(서버 시각)을 갱신합니다. 웹페이지는 이 값이 6초 넘게 멈추면 "기기 오프라인"으로 표시합니다.
- DB 쓰기는 터치 상태가 바뀌거나 2초가 지났을 때만 합니다. 0.2초마다 쓰면 실시간 알림이 쏟아집니다.

### 로컬에서 바꾼 LED가 되돌아가는 문제

보드 페이지에서 LED를 켰는데, 바로 다음 클라우드 응답이 "꺼짐"이면 LED가 다시 꺼집니다. 그래서 로컬 변경은 `ledPending` 표시를 해 두고 **다음 보고에 실어 먼저 올린 뒤** 응답을 받습니다.

```cpp
if (pushLed) body += String(",\"p_led\":") + (ledOn ? "true" : "false");
...
if (pushLed) ledPending = false;
```

### 웹페이지 배포

Supabase에 연결하는 HTML 한 장을 GitHub Pages에 올렸습니다. 처음엔 claude.ai 아티팩트로 올리려 했지만, 아티팩트는 외부 서버 연결이 막혀 있어서 GitHub Pages로 바꿨습니다.

---

## 4. 카메라 스트리밍

### MJPEG

카메라가 찍은 JPEG를 한 응답 안에서 경계 문자열(`--xiaoframe`)로 구분해 계속 보내면, 브라우저는 `<img src=".../stream">` 하나로 영상을 보여 줍니다.

```
HTTP/1.1 200 OK
Content-Type: multipart/x-mixed-replace;boundary=xiaoframe

--xiaoframe
Content-Type: image/jpeg
Content-Length: 12494

<JPEG 바이트>
--xiaoframe
...
```

### 서버 구조를 바꾼 이유

| | 처음 (`esp_http_server`) | 바꾼 뒤 (`WiFiServer` + 작업) |
|---|---|---|
| 동시 시청 | 1명 (스트림이 서버를 점유) | 3명 (`MAX_STREAMS`) |
| 4번째 시청자 | 무한 대기 | 바로 `503` |
| 긴 요청 헤더 | 1024바이트 넘으면 `431` | 첫 줄만 읽고 나머지는 건너뜀 |
| 스트림 중 사진 저장 | 막힘 | 별도 작업이라 `200` |

```cpp
// 접속이 들어오면 시청자마다 FreeRTOS 작업을 따로 만든다
WiFiClient *client = new WiFiClient(incoming);
xTaskCreatePinnedToCore(camClientTask, "camClient", 8192, client, 2, NULL, 0);
```

측정값: VGA 1명 5초에 118프레임(약 24fps), 3명 동시 시청 시 각자 6초에 약 50프레임.

### PSRAM 빌드 옵션

카메라 프레임 버퍼는 PSRAM에 둡니다. `arduino-cli`의 기본값은 `PSRAM=disabled`라서 반드시 `--fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi`로 빌드해야 합니다.

### 화면이 까맣게 나온 이유: Arduino 전처리기

`.ino` 파일은 컴파일 전에 전처리기가 함수 선언(프로토타입)을 자동으로 만들어 위쪽에 넣습니다. 그런데 HTML 문자열 안의 자바스크립트 `function start() { ... }`까지 C++ 함수로 오인해서, **HTML 한가운데에** 이런 줄을 끼워 넣었습니다.

```
document.getElementById('host').textContent = location.host;
#line 107 "C:\\Users\\...\\SupabaseDashboard.ino"
function start();
```

브라우저는 이 줄에서 문법 오류를 내고 스크립트 전체를 실행하지 않았습니다. 사진 저장은 스크립트 없는 링크라 동작했고, 영상만 요청되지 않았습니다. curl로는 서버가 정상이라 보이지 않았고, **브라우저 콘솔**을 보고 찾았습니다.

**해결:** HTML은 `.h` 파일(`dashboard_html.h`)에 둡니다. `.h`는 전처리기가 건드리지 않습니다.

---

## 5. AP 모드

보드가 직접 Wi-Fi를 띄웁니다. 인터넷이 없는 곳에서도 휴대폰으로 제어할 수 있습니다.

```cpp
WiFi.mode(WIFI_AP);
WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);       // 192.168.4.1
WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);         // 채널 1, 이름 보임, 최대 4대
```

휴대폰에서 생기는 일:

- "인터넷 연결 없음" 알림 → **연결 유지**를 눌러야 다른 Wi-Fi로 넘어가지 않습니다.
- 모바일 데이터가 켜져 있으면 `192.168.4.1` 요청이 LTE로 가는 경우가 있어 잠시 꺼야 할 수 있습니다.
- 휴대폰이 확인용 주소(예: `/generate_204`)를 요청하면 대시보드로 보내도록 `onNotFound`에서 리다이렉트했습니다.

**"Wi-Fi가 안 잡힌다"의 원인:** 펌웨어를 업로드하기 전이었습니다. PC에서 Wi-Fi 목록을 강제로 다시 검색해서 `XIAO-ESP32S3`가 신호 99%로 보이는 것을 확인했습니다(Windows는 연결 중일 때 목록을 잘 갱신하지 않습니다).

---

## 6. BLE

### GATT 구성

| 항목 | UUID | 권한 | 값 |
|---|---|---|---|
| 서비스 | `d429d54e-...` | – | – |
| 터치 특성 | `cc6ffe3a-...` | 읽기 · 알림 | `"0,16940,16934"` (터치 여부, 현재값, 기준값) |
| LED 특성 | `6083a55f-...` | 읽기 · 쓰기 · 알림 | `"1"` / `"0"` (쓰기는 `on`·`off`·`toggle`·`0x01`도 받음) |

값을 **사람이 읽을 수 있는 문자열**로 정해서 nRF Connect 같은 범용 앱에서도 바로 이해할 수 있게 했습니다. 각 특성에는 설명(0x2901 디스크립터)도 붙였습니다.

### Notify

연결된 동안 터치 값은 0.2초마다, 터치 상태가 바뀌면 즉시 알림으로 보냅니다. LED 쓰기 콜백 안에서는 바로 알림을 보내지 않고 표시만 해 둔 뒤 `loop()`에서 보냅니다(BLE 스택 콜백 안에서 오래 걸리는 일을 피함).

```cpp
void onWrite(BLECharacteristic *c) {
  ...
  ledChanged = true;          // loop()에서 ledChar->notify()
}
```

### 광고 패킷 31바이트

광고 패킷은 31바이트뿐인데, 128비트 서비스 UUID가 18바이트를 차지해서 이름까지 넣으면 넘칩니다. 그래서 이름은 **스캔 응답**에 넣었습니다(`setScanResponse(true)`).

### Web Bluetooth

[ble.html](BLEDashboard/ble.html)은 브라우저에서 BLE 기기에 직접 연결합니다.

```js
device = await navigator.bluetooth.requestDevice({ filters: [{ services: [SERVICE_UUID] }] });
const service = await (await device.gatt.connect()).getPrimaryService(SERVICE_UUID);
await touchChar.startNotifications();
await ledChar.writeValue(new TextEncoder().encode('1'));
```

- **HTTPS 페이지에서만** 동작해서 GitHub Pages에 올렸습니다.
- 안드로이드 Chrome은 지원, 아이폰 Safari는 미지원(Bluefy 앱이나 nRF Connect 사용).

### PC에서 검증

휴대폰으로 보기 전에 PC 블루투스와 Python `bleak`로 실제 연결해 확인했습니다: 검색·연결 성공, 터치 읽기, 알림 약 200ms 간격, LED 쓰기 4종(`1`, `off`, `toggle`, `0x00`) 모두 반영.

---

## 7. 연결 방식 비교

| | 같은 Wi-Fi | Supabase | AP 모드 | BLE |
|---|---|---|---|---|
| 인터넷 | 불필요 | **필요** | 불필요 | 불필요 |
| 거리 | 공유기 범위 | 어디서나 | 약 수십 m | 약 10m |
| 휴대폰 동시 접속 | 여러 대 | 여러 대 | 최대 4대 | **1대** |
| 휴대폰의 인터넷 | 유지 | 유지 | 끊김(보드 Wi-Fi에 붙음) | 유지 |
| 지연 | 짧음 | 수백 ms | 짧음 | 짧음 |
| 카메라 영상 | 가능 | 링크만(같은 Wi-Fi에서) | 가능(코드 추가 시) | 사실상 불가(대역폭) |

---

## 8. AI 에이전트와 작업하며 배운 것

- **확인할 수 없는 것은 확인했다고 말하지 않기.** 실제 LED 점등, 휴대폰 화면은 에이전트가 볼 수 없어서 매번 사람이 확인했습니다.
- **고쳤으면 바로 업로드하고, 동작 흔적으로 확인하기.** "안 된다"의 원인 두 번이 모두 "업로드 전"이었습니다. DB 접속 기록, 시리얼 로그, Wi-Fi 검색처럼 **눈에 보이는 증거**로 확인했습니다.
- **curl이 통과해도 브라우저는 실패할 수 있다.** 서버 응답과 페이지 스크립트는 따로 확인해야 합니다.
- **겪은 문제는 규칙으로 남긴다.** [AGENTS.md](AGENTS.md)에 "하지 말 것"으로 적어 두면 다음 작업(사람이든 에이전트든)이 같은 실수를 반복하지 않습니다.
- **비밀 정보는 처음부터 분리.** Wi-Fi 비밀번호와 장치 시크릿은 `secrets.h`에만 두고 `.gitignore`로 제외했습니다.
