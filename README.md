# XIAO ESP32-S3 Lab

Seeed **XIAO ESP32-S3 Sense** 보드 하나로 LED와 터치센서를 다루고, 같은 기능을 **Wi-Fi 웹서버 → 클라우드(Supabase) 원격 제어 → 카메라 스트리밍 → AP 모드 → BLE** 순서로 연결 방식을 바꿔 가며 스마트폰에서 제어해 본 학습 기록입니다.
코드는 AI 코딩 에이전트(Claude Code)와 함께 작성했고, `arduino-cli`로 노트북에서 보드에 바로 업로드했습니다.

![ESP32](https://img.shields.io/badge/ESP32--S3-E7352C?style=flat-square&logo=espressif&logoColor=white)
![C++](https://img.shields.io/badge/C++-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![Supabase](https://img.shields.io/badge/Supabase-3FCF8E?style=flat-square&logo=supabase&logoColor=white)
![Bluetooth](https://img.shields.io/badge/BLE-0082FC?style=flat-square&logo=bluetooth&logoColor=white)
![GitHub Pages](https://img.shields.io/badge/GitHub_Pages-222222?style=flat-square&logo=githubpages&logoColor=white)
![arduino-cli](https://img.shields.io/badge/arduino--cli-esp32_3.3.12-00979D?style=flat-square)

**바로 써 보기** · [원격 대시보드](https://leesm3528-netizen.github.io/xiao-dashboard/) · [BLE 제어 페이지](https://leesm3528-netizen.github.io/xiao-dashboard/ble.html) (보드가 해당 펌웨어로 켜져 있어야 동작)

> 자세한 설명과 코드 예시는 [STUDY_NOTES.md](STUDY_NOTES.md)에 정리했습니다.

---

## 한눈에 보기 — 연결 방식 5가지

같은 LED·터치센서를 연결 방식만 바꿔서 제어했습니다.

| 방식 | 스케치 | 인터넷 | 스마트폰에서 접속하는 법 | 확인 결과 |
|---|---|---|---|---|
| **같은 Wi-Fi 웹서버** | [WebDashboard](WebDashboard/WebDashboard.ino) | 필요 없음 (공유기만) | 집 Wi-Fi에서 `http://xiao.local/` | – |
| **클라우드 원격 제어** | [SupabaseDashboard](SupabaseDashboard/SupabaseDashboard.ino) | 필요 | 어디서나 [원격 대시보드](https://leesm3528-netizen.github.io/xiao-dashboard/) | LED 원격 점등 확인 |
| **카메라 스트리밍** | [SupabaseDashboard](SupabaseDashboard/camera_stream.h) | 필요 없음 (같은 Wi-Fi) | `http://<보드 IP>/` | 3명 동시 시청, 약 24fps(VGA, 1명) |
| **AP 모드** | [APDashboard](APDashboard/APDashboard.ino) | 없음 | 보드가 띄운 Wi-Fi에 접속 → `http://192.168.4.1/` | 휴대폰 접속, LED 점등 확인 |
| **BLE** | [BLEDashboard](BLEDashboard/BLEDashboard.ino) | 없음 | [BLE 페이지](https://leesm3528-netizen.github.io/xiao-dashboard/ble.html) 또는 nRF Connect | 휴대폰 연결, LED·터치값 확인 |

---

## 오늘의 학습 키워드

### 보드와 핀

| 키워드 | 한 줄 정리 | 스케치 |
|---|---|---|
| **XIAO ESP32-S3 Sense** | 엄지손톱만 한 ESP32-S3 보드. Wi-Fi·BLE 내장, 8MB PSRAM, 카메라 확장보드(Sense) | – |
| **유저 LED (GPIO21)** | **active-low**: `LOW`가 켜짐, `HIGH`가 꺼짐 | [Blink](Blink/Blink.ino) |
| **정전식 터치 (D0 = GPIO1)** | `touchRead()`로 읽는다. ESP32-S3는 손을 대면 값이 **올라간다** (평소 약 17,000) | [TouchLED](TouchLED/TouchLED.ino) |
| **기준값 보정** | 부팅 때 20번 읽어 평균을 기준값으로 잡고, 1.3배를 넘으면 터치로 판정. 터치 안 할 때는 기준값을 천천히 따라간다 | [TouchLED](TouchLED/TouchLED.ino) |

### 네트워크

| 키워드 | 한 줄 정리 | 스케치 |
|---|---|---|
| **STA 모드** | 보드가 집 공유기에 **접속하는** 쪽. 공유기가 IP를 준다 | [WebDashboard](WebDashboard/WebDashboard.ino) |
| **AP 모드** | 보드가 직접 **Wi-Fi를 띄우는** 쪽. 인터넷 없는 공유기가 되고 IP는 `192.168.4.1` | [APDashboard](APDashboard/APDashboard.ino) |
| **mDNS** | IP 대신 `xiao.local` 같은 이름으로 접속 | [WebDashboard](WebDashboard/WebDashboard.ino) |
| **REST API** | `/api/status`(JSON 상태), `/api/led?state=on` 같은 주소로 기능을 노출하고, 페이지 JS가 `fetch`로 호출 | [WebDashboard](WebDashboard/WebDashboard.ino) |

### 클라우드 (Supabase)

| 키워드 | 한 줄 정리 | 파일 |
|---|---|---|
| **왜 클라우드?** | 보드는 공유기 안쪽(사설 IP)이라 밖에서 직접 못 들어온다. 보드와 휴대폰이 둘 다 **바깥의 DB**에 접속해서 값을 주고받는다 | – |
| **RLS + 열 단위 권한** | 공개 키로는 `led_on` 열만 바꿀 수 있게 막았다. 터치값은 웹에서 못 바꾼다 | [xiao_schema.sql](supabase/xiao_schema.sql) |
| **RPC (보드 전용 함수)** | 보드는 `xiao_report()` 하나만 호출. 장치 시크릿이 맞아야 저장되고, 응답으로 LED 목표값을 받는다 | [xiao_schema.sql](supabase/xiao_schema.sql) |
| **Realtime** | 값이 바뀌면 DB가 웹페이지로 바로 밀어 준다(폴링 없음) | [dashboard.html](SupabaseDashboard/dashboard.html) |

### 카메라

| 키워드 | 한 줄 정리 | 파일 |
|---|---|---|
| **PSRAM** | 카메라 프레임 버퍼를 담는 외장 램. 빌드 옵션 `PSRAM=opi`가 없으면 카메라가 안 뜬다 | – |
| **MJPEG 스트림** | JPEG 사진을 `multipart/x-mixed-replace`로 계속 이어 보내면 `<img>` 하나로 영상이 된다 | [camera_stream.h](SupabaseDashboard/camera_stream.h) |
| **FreeRTOS 작업** | 시청자마다 작업(task)을 따로 만들어서 한 명이 보는 동안 다른 요청이 막히지 않게 했다 | [camera_stream.h](SupabaseDashboard/camera_stream.h) |

### BLE

| 키워드 | 한 줄 정리 | 파일 |
|---|---|---|
| **GATT 서비스 / 특성** | 서비스(UUID) 안에 특성(characteristic) 두 개: 터치(읽기·알림), LED(읽기·쓰기·알림) | [BLEDashboard](BLEDashboard/BLEDashboard.ino) |
| **Notify** | 휴대폰이 묻지 않아도 보드가 값이 바뀔 때 먼저 보내 준다(0.2초 간격) | [BLEDashboard](BLEDashboard/BLEDashboard.ino) |
| **Web Bluetooth** | 브라우저에서 BLE 기기에 직접 연결. **HTTPS 필수**, 안드로이드 Chrome 지원(아이폰 Safari 미지원) | [ble.html](BLEDashboard/ble.html) |

---

## 진행 과정 · 1일차 (10/23)

1. **Blink · TouchLED** — GPIO21 LED가 active-low인 것을 확인하고, D0 터치로 LED를 켜는 스케치를 만들었습니다.
2. **WebDashboard** — 보드가 집 Wi-Fi에 접속해 웹서버를 열고, 같은 Wi-Fi의 휴대폰에서 LED를 켜고 터치 상태를 봤습니다.
3. **Supabase 원격 제어** — 외부망에서도 되도록 보드와 웹페이지를 Supabase로 연결했습니다. 웹페이지는 GitHub Pages에 올렸습니다.
4. **카메라 스트리밍** — 카메라를 켜고 MJPEG 영상을 같은 Wi-Fi에서 볼 수 있게 했습니다. 처음엔 화면이 까맣게 나와서 원인을 찾았습니다(아래 트러블슈팅).
5. **다른 PC에서 성공한 구조로 교체** — 카메라 서버를 `WiFiServer` + 시청자별 작업 구조로 바꿔 3명 동시 시청과 긴 헤더 문제를 해결하고, 비밀번호는 `secrets.h`로 분리했습니다.
6. **AP 모드** — 인터넷 없이 보드가 Wi-Fi를 띄우고 휴대폰이 접속해서 LED·터치값을 다뤘습니다.
7. **BLE** — 휴대폰과 블루투스로 직접 연결해 터치값 읽기와 LED 제어를 했습니다.

---

## 트러블슈팅

| 증상 | 원인 | 해결 |
|---|---|---|
| 원격 대시보드 버튼을 눌러도 LED가 안 켜짐 | 보드에 **이전 펌웨어**가 그대로 있었다. DB에 보드 접속 기록이 0건 | 새 펌웨어 업로드. 이후 "고치면 바로 업로드하고 접속 기록 확인"을 규칙으로 |
| 카메라 사진은 되는데 영상은 까만 화면 | Arduino 전처리기가 HTML 안 JS의 `function start()`를 C++ 함수로 오인해 `#line ...`을 끼워 넣음 → 스크립트 전체가 문법 오류 | HTML을 `.ino`가 아닌 **`.h` 파일**로 분리 |
| 영상이 한 기기에서만 보임 | `esp_http_server`는 스트림 하나가 서버 작업을 계속 점유 | 시청자마다 FreeRTOS 작업을 만드는 `WiFiServer` 서버로 교체 |
| (다른 PC) 휴대폰에서 `431 Header fields are too long` | `esp_http_server` 헤더 길이 제한(1024B)이 코어에 고정 | 요청 첫 줄만 읽고 헤더는 길이와 상관없이 건너뜀 |
| 휴대폰에서 보드 Wi-Fi(AP)가 안 보임 | AP 펌웨어를 컴파일만 하고 **업로드 전**이었다 | 업로드 후 PC에서 Wi-Fi 강제 재검색으로 신호 99% 확인 |
| `arduino-cli monitor`에 `timeout`을 걸면 로그가 비어 나옴 | Git Bash 환경 문제 | PowerShell `System.IO.Ports.SerialPort`로 읽기 |

---

## 스케치

| 폴더 | 설명 | 빌드 옵션 |
|---|---|---|
| [Blink](Blink/) | LED 켜기·끄기를 `delay` 없이 반복 → 너무 빨라서 눈에는 계속 켜진 것처럼 보인다 | 기본 |
| [TouchLED](TouchLED/) | D0을 터치하는 동안 LED 켜기, 시리얼로 기준값·현재값 출력 | 기본 |
| [WebDashboard](WebDashboard/) | 같은 Wi-Fi 웹 대시보드 (LED, 터치) | 기본 |
| [SupabaseDashboard](SupabaseDashboard/) | 로컬 대시보드 + 카메라(80/81번 포트) + Supabase 원격 동기화. `dashboard.html`은 GitHub Pages용 원격 대시보드 | `PSRAM=opi` |
| [APDashboard](APDashboard/) | AP 모드 대시보드 (`192.168.4.1`) | 기본 |
| [BLEDashboard](BLEDashboard/) | BLE 서버 + 휴대폰용 Web Bluetooth 페이지(`ble.html`) | 기본 |
| [supabase](supabase/) | 원격 제어용 테이블·권한·RPC를 한 번에 만드는 SQL | – |

---

## 실행 방법

```bash
# 1) 보드 지원 설치 (한 번만)
arduino-cli core install esp32:esp32@3.3.12

# 2) 비밀 정보 파일 만들기 (Wi-Fi·Supabase를 쓰는 스케치만)
cp SupabaseDashboard/secrets.example.h SupabaseDashboard/secrets.h   # 값 채우기

# 3) 컴파일 · 업로드 (포트는 arduino-cli board list로 확인)
FQBN=esp32:esp32:XIAO_ESP32S3
arduino-cli compile --fqbn $FQBN BLEDashboard
arduino-cli upload  -p COM4 --fqbn $FQBN BLEDashboard

# 카메라를 쓰는 SupabaseDashboard는 PSRAM 옵션 필수
arduino-cli upload -p COM4 --fqbn $FQBN:PSRAM=opi SupabaseDashboard
```

Supabase를 쓰려면 [supabase/xiao_schema.sql](supabase/xiao_schema.sql)을 SQL 편집기에서 실행하고, `<DEVICE_SECRET>` 자리와 `secrets.h`에 같은 값을 넣습니다.

> 부팅하는 몇 초 동안은 D0 터치센서를 만지지 마세요. 이때 기준값을 잽니다.

---

## 폴더 구조

```
xiao-esp32s3-lab/
├── Blink/ · TouchLED/          기초 예제
├── WebDashboard/               같은 Wi-Fi 웹서버
├── SupabaseDashboard/          원격 제어 + 카메라
│   ├── camera_stream.h         카메라 서버 (81번 포트)
│   ├── dashboard_html.h        보드가 보여 주는 페이지
│   └── dashboard.html          GitHub Pages 원격 대시보드
├── APDashboard/                AP 모드
├── BLEDashboard/               BLE + ble.html
├── supabase/xiao_schema.sql    DB 구성
├── AGENTS.md                   AI 에이전트 작업 규칙 (실제로 겪은 문제 정리)
└── STUDY_NOTES.md              학습 정리
```

## 참고

- 보드 공식 문서: [Seeed Studio XIAO ESP32-S3 위키](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- 원격 대시보드 배포 저장소: [xiao-dashboard](https://github.com/leesm3528-netizen/xiao-dashboard)
