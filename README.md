# XIAO ESP32-S3 Lab

Seeed **XIAO ESP32-S3 Sense** 보드 하나로 LED와 터치센서를 다루고, 같은 기능을 **Wi-Fi 웹서버 → 클라우드(Supabase) 원격 제어 → 카메라 스트리밍 → AP 모드 → BLE → MQTT** 순서로 연결 방식을 바꿔 가며 스마트폰에서 제어해 본 학습 기록입니다.
3일차에는 카메라로 사진을 모아 **Edge Impulse**로 이미지 분류 모델(민트 / 물병)을 학습하고, 보드가 직접 카메라 영상을 분류하게 했습니다(추론 약 185ms).
코드는 AI 코딩 에이전트(Claude Code)와 함께 작성했고, `arduino-cli`로 노트북에서 보드에 바로 업로드했습니다.

![ESP32](https://img.shields.io/badge/ESP32--S3-E7352C?style=flat-square&logo=espressif&logoColor=white)
![C++](https://img.shields.io/badge/C++-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![Supabase](https://img.shields.io/badge/Supabase-3FCF8E?style=flat-square&logo=supabase&logoColor=white)
![Bluetooth](https://img.shields.io/badge/BLE-0082FC?style=flat-square&logo=bluetooth&logoColor=white)
![MQTT](https://img.shields.io/badge/MQTT_(Mosquitto)-660066?style=flat-square&logo=mqtt&logoColor=white)
![Edge Impulse](https://img.shields.io/badge/Edge_Impulse-3B47C2?style=flat-square)
![GitHub Pages](https://img.shields.io/badge/GitHub_Pages-222222?style=flat-square&logo=githubpages&logoColor=white)
![arduino-cli](https://img.shields.io/badge/arduino--cli-esp32_3.3.12-00979D?style=flat-square)

**바로 써 보기** · [원격 대시보드](https://leesm3528-netizen.github.io/xiao-dashboard/) · [BLE 제어 페이지](https://leesm3528-netizen.github.io/xiao-dashboard/ble.html) (보드가 해당 펌웨어로 켜져 있어야 동작)

> 자세한 설명과 코드 예시는 [STUDY_NOTES.md](STUDY_NOTES.md)에 정리했습니다.

---

## 한눈에 보기 — 연결 방식 6가지

같은 LED·터치센서를 연결 방식만 바꿔서 제어했습니다.

| 방식 | 스케치 | 인터넷 | 스마트폰에서 접속하는 법 | 확인 결과 |
| --- | --- | --- | --- | --- |
| **같은 Wi-Fi 웹서버** | [WebDashboard](WebDashboard/WebDashboard.ino) | 필요 없음 (공유기만) | 집 Wi-Fi에서 `http://xiao.local/` | – |
| **클라우드 원격 제어** | [SupabaseDashboard](SupabaseDashboard/SupabaseDashboard.ino) | 필요 | 어디서나 [원격 대시보드](https://leesm3528-netizen.github.io/xiao-dashboard/) | LED 원격 점등 확인 |
| **카메라 스트리밍** | [SupabaseDashboard](SupabaseDashboard/camera_stream.h) | 필요 없음 (같은 Wi-Fi) | `http://<보드 IP>/` | 3명 동시 시청, 약 24fps(VGA, 1명) |
| **AP 모드** | [APDashboard](APDashboard/APDashboard.ino) | 없음 | 보드가 띄운 Wi-Fi에 접속 → `http://192.168.4.1/` | 휴대폰 접속, LED 점등 확인 |
| **BLE** | [BLEDashboard](BLEDashboard/BLEDashboard.ino) | 없음 | [BLE 페이지](https://leesm3528-netizen.github.io/xiao-dashboard/ble.html) 또는 nRF Connect | 휴대폰 연결, LED·터치값 확인 |
| **MQTT (로컬 브로커)** | [MqttDashboard](MqttDashboard/) | 필요 없음 (같은 Wi-Fi, PC에 Mosquitto) | 브라우저 `http://<PC IP>:8080` 또는 **IoT MQTT Panel** 앱 | 휴대폰 앱이 퍼블리셔로 접속, 앱으로 LED 토글·슬라이더로 터치값 확인 |

---

## 오늘의 학습 키워드

### 보드와 핀

| 키워드 | 한 줄 정리 | 스케치 |
| --- | --- | --- |
| **XIAO ESP32-S3 Sense** | 엄지손톱만 한 ESP32-S3 보드. Wi-Fi·BLE 내장, 8MB PSRAM, 카메라 확장보드(Sense) | – |
| **유저 LED (GPIO21)** | **active-low**: `LOW`가 켜짐, `HIGH`가 꺼짐 | [Blink](Blink/Blink.ino) |
| **정전식 터치 (D0 = GPIO1)** | `touchRead()`로 읽는다. ESP32-S3는 손을 대면 값이 **올라간다** (평소 약 17,000) | [TouchLED](TouchLED/TouchLED.ino) |
| **기준값 보정** | 부팅 때 20번 읽어 평균을 기준값으로 잡고, 1.3배를 넘으면 터치로 판정. 터치 안 할 때는 기준값을 천천히 따라간다 | [TouchLED](TouchLED/TouchLED.ino) |

### 네트워크

| 키워드 | 한 줄 정리 | 스케치 |
| --- | --- | --- |
| **STA 모드** | 보드가 집 공유기에 **접속하는** 쪽. 공유기가 IP를 준다 | [WebDashboard](WebDashboard/WebDashboard.ino) |
| **AP 모드** | 보드가 직접 **Wi-Fi를 띄우는** 쪽. 인터넷 없는 공유기가 되고 IP는 `192.168.4.1` | [APDashboard](APDashboard/APDashboard.ino) |
| **mDNS** | IP 대신 `xiao.local` 같은 이름으로 접속 | [WebDashboard](WebDashboard/WebDashboard.ino) |
| **REST API** | `/api/status`(JSON 상태), `/api/led?state=on` 같은 주소로 기능을 노출하고, 페이지 JS가 `fetch`로 호출 | [WebDashboard](WebDashboard/WebDashboard.ino) |

### 클라우드 (Supabase)

| 키워드 | 한 줄 정리 | 파일 |
| --- | --- | --- |
| **왜 클라우드?** | 보드는 공유기 안쪽(사설 IP)이라 밖에서 직접 못 들어온다. 보드와 휴대폰이 둘 다 **바깥의 DB**에 접속해서 값을 주고받는다 | – |
| **RLS + 열 단위 권한** | 공개 키로는 `led_on` 열만 바꿀 수 있게 막았다. 터치값은 웹에서 못 바꾼다 | [xiao_schema.sql](supabase/xiao_schema.sql) |
| **RPC (보드 전용 함수)** | 보드는 `xiao_report()` 하나만 호출. 장치 시크릿이 맞아야 저장되고, 응답으로 LED 목표값을 받는다 | [xiao_schema.sql](supabase/xiao_schema.sql) |
| **Realtime** | 값이 바뀌면 DB가 웹페이지로 바로 밀어 준다(폴링 없음) | [dashboard.html](SupabaseDashboard/dashboard.html) |

### 카메라

| 키워드 | 한 줄 정리 | 파일 |
| --- | --- | --- |
| **PSRAM** | 카메라 프레임 버퍼를 담는 외장 램. 빌드 옵션 `PSRAM=opi`가 없으면 카메라가 안 뜬다 | – |
| **MJPEG 스트림** | JPEG 사진을 `multipart/x-mixed-replace`로 계속 이어 보내면 `<img>` 하나로 영상이 된다 | [camera_stream.h](SupabaseDashboard/camera_stream.h) |
| **FreeRTOS 작업** | 시청자마다 작업(task)을 따로 만들어서 한 명이 보는 동안 다른 요청이 막히지 않게 했다 | [camera_stream.h](SupabaseDashboard/camera_stream.h) |

### BLE

| 키워드 | 한 줄 정리 | 파일 |
| --- | --- | --- |
| **GATT 서비스 / 특성** | 서비스(UUID) 안에 특성(characteristic) 두 개: 터치(읽기·알림), LED(읽기·쓰기·알림) | [BLEDashboard](BLEDashboard/BLEDashboard.ino) |
| **Notify** | 휴대폰이 묻지 않아도 보드가 값이 바뀔 때 먼저 보내 준다(0.2초 간격) | [BLEDashboard](BLEDashboard/BLEDashboard.ino) |
| **Web Bluetooth** | 브라우저에서 BLE 기기에 직접 연결. **HTTPS 필수**, 안드로이드 Chrome 지원(아이폰 Safari 미지원) | [ble.html](BLEDashboard/ble.html) |

### Edge Impulse (이미지 분류)

| 키워드 | 한 줄 정리 | 파일 |
| --- | --- | --- |
| **수집 대시보드** | PC 파이썬 서버가 보드 `/capture`를 받아 `라벨.시각.jpg`로 저장. 연속 촬영·삭제(휴지통)·되돌리기·ZIP | [EICollector](EICollector/) |
| **Edge Impulse REST API** | 브라우저 없이 API 키로 업로드 → 특징 추출 → 학습 → Arduino 라이브러리 빌드까지 스크립트로 | [EIModel/scripts](EIModel/scripts/) |
| **int8 양자화** | 보드에서 빠르게 돌리려고 가중치를 8비트로 줄인다. 모델에 따라 정확도가 크게 떨어질 수 있어서 int8 점수로 모델을 골랐다 | [EIModel](EIModel/README.md) |
| **전처리 맞추기** | 학습 사진과 같은 240×240 JPEG를 RGB로 풀고 96×96으로 줄여 모델에 넣는다 | [EIInference](EIInference/EIInference.ino) |
| **온디바이스 추론** | PC·클라우드 없이 보드 칩이 직접 분류. ESP32-S3의 SIMD 가속(ESP-NN)으로 신경망 약 84ms, 전체 약 185ms | [EIInference](EIInference/README.md) |
| **메모리 정렬** | SIMD 명령은 16바이트 단위로 맞춘 주소만 제대로 다룬다. 정렬 안 된 버퍼를 주면 앞쪽 메모리를 덮어써 힙이 깨진다 | [STUDY_NOTES](STUDY_NOTES.md#10-edge-impulse-보드에서-추론하기) |

### MQTT

| 키워드 | 한 줄 정리 | 파일 |
| --- | --- | --- |
| **브로커 (Mosquitto)** | 메시지 우체국. 보드·웹·휴대폰은 서로 직접 연결하지 않고 PC의 브로커에만 붙는다 | [mosquitto.conf](MqttDashboard/broker/mosquitto.conf) |
| **토픽 · 발행 · 구독** | `xiao/touch`처럼 주소(토픽)에 값을 **발행(publish)** 하면, 그 토픽을 **구독(subscribe)** 한 모두가 받는다. `#`은 "전부" | [MqttDashboard.ino](MqttDashboard/MqttDashboard.ino) |
| **retained · LWT** | `retained`는 마지막 값을 브로커가 기억해 새로 접속한 쪽에 바로 준다. LWT(유언)는 보드가 갑자기 끊기면 브로커가 대신 `offline`을 발행 | [MqttDashboard.ino](MqttDashboard/MqttDashboard.ino) |
| **WebSocket 리스너** | 브라우저는 MQTT(TCP)로 직접 못 붙어서 브로커에 WebSocket 포트(9001)를 따로 열고 `mqtt.js`로 접속 | [index.html](MqttDashboard/web/index.html) |

---

## 진행 과정 · 1일차 (10/23)

1. **Blink · TouchLED** — GPIO21 LED가 active-low인 것을 확인하고, D0 터치로 LED를 켜는 스케치를 만들었습니다.
2. **WebDashboard** — 보드가 집 Wi-Fi에 접속해 웹서버를 열고, 같은 Wi-Fi의 휴대폰에서 LED를 켜고 터치 상태를 봤습니다.
3. **Supabase 원격 제어** — 외부망에서도 되도록 보드와 웹페이지를 Supabase로 연결했습니다. 웹페이지는 GitHub Pages에 올렸습니다.
4. **카메라 스트리밍** — 카메라를 켜고 MJPEG 영상을 같은 Wi-Fi에서 볼 수 있게 했습니다. 처음엔 화면이 까맣게 나와서 원인을 찾았습니다(아래 트러블슈팅).
5. **다른 PC에서 성공한 구조로 교체** — 카메라 서버를 `WiFiServer` + 시청자별 작업 구조로 바꿔 3명 동시 시청과 긴 헤더 문제를 해결하고, 비밀번호는 `secrets.h`로 분리했습니다.
6. **AP 모드** — 인터넷 없이 보드가 Wi-Fi를 띄우고 휴대폰이 접속해서 LED·터치값을 다뤘습니다.
7. **BLE** — 휴대폰과 블루투스로 직접 연결해 터치값 읽기와 LED 제어를 했습니다.

## 진행 과정 · 2일차 (11/2 ~ 11/3)

<!-- markdownlint-disable MD029 -- 1일차에서 이어지는 번호 -->
8. **MQTT 로컬 대시보드** — PC에 설치한 Mosquitto를 브로커로 쓰고, 보드는 터치값을 발행하고 LED 명령을 구독합니다. 웹 대시보드는 터치값·30초 그래프·LED 버튼을 보여 줍니다.
9. **휴대폰을 퍼블리셔로** — IoT MQTT Panel 앱을 같은 브로커에 붙였습니다. 웹 대시보드는 `#`을 구독해서 메시지를 보낸 기기(xiao, phone)를 표로 보여 줍니다.
10. **휴대폰 슬라이더로 터치값 보기** — 앱 위젯은 JSON을 못 읽어서 보드가 숫자만 담은 `xiao/touch/value`도 함께 발행하게 했습니다.
11. **느려진 원인 찾기** — LED가 한 박자 늦고 터치 메시지가 끊기는 문제를 시간 측정으로 좁혀서 **USB 시리얼 출력이 막혀 `loop()`가 멈춘 것**을 찾았습니다(아래 트러블슈팅).

<!-- markdownlint-enable MD029 -->

## 진행 과정 · 3일차 (11/4 ~ 11/5)

<!-- markdownlint-disable MD029 -- 2일차에서 이어지는 번호 -->
12. **문서 lint** — markdownlint 설정 파일을 만들고 문서 오류를 고쳤습니다.
13. **사진 수집 대시보드** — 카메라 전용 펌웨어(`EICamera`)와 PC 수집 서버(`EICollector`)로 클래스별 촬영·연속 촬영·삭제·ZIP 다운로드를 만들었습니다. 민트 100장, 물병 101장을 모았습니다.
14. **모델 학습** — `.env`의 API 키로 Edge Impulse에 업로드하고 학습했습니다. MobileNetV2는 int8로 바꾸면 정확도가 76~88%로 떨어져서, int8에서도 점수가 유지되는 MobileNetV1 0.25를 골랐습니다. 테스트 정확도 90%(40장 중 36장).
15. **원격 대시보드 복구** — 외부 대시보드가 "오프라인"이었던 원인은 보드에 수집 펌웨어가 올라가 있었기 때문이었습니다. 운영 펌웨어를 다시 올려 LED 원격 제어를 확인했습니다.
16. **추론 펌웨어 작성** — 보드에서 분류하고 웹 페이지·시리얼·LED로 결과를 보여 주는 `EIInference`를 작성했습니다.
17. **추론 펌웨어 업로드** — `--jobs 2`로 동시 컴파일 수를 줄여 SDK 첫 컴파일(파일 848개)을 끝냈습니다. 업로드 직후 모델 준비 단계에서 재부팅이 반복되어, 시리얼 로그와 디버그 출력으로 원인을 좁혀 Edge Impulse 라이브러리 두 곳을 고쳤습니다(아래 트러블슈팅). 이제 <http://192.168.0.25/> 에서 영상과 분류 결과가 0.2초마다 나옵니다.

<!-- markdownlint-enable MD029 -->

---

## 트러블슈팅

| 증상 | 원인 | 해결 |
| --- | --- | --- |
| 원격 대시보드 버튼을 눌러도 LED가 안 켜짐 | 보드에 **이전 펌웨어**가 그대로 있었다. DB에 보드 접속 기록이 0건 | 새 펌웨어 업로드. 이후 "고치면 바로 업로드하고 접속 기록 확인"을 규칙으로 |
| 카메라 사진은 되는데 영상은 까만 화면 | Arduino 전처리기가 HTML 안 JS의 `function start()`를 C++ 함수로 오인해 `#line ...`을 끼워 넣음 → 스크립트 전체가 문법 오류 | HTML을 `.ino`가 아닌 **`.h` 파일**로 분리 |
| 영상이 한 기기에서만 보임 | `esp_http_server`는 스트림 하나가 서버 작업을 계속 점유 | 시청자마다 FreeRTOS 작업을 만드는 `WiFiServer` 서버로 교체 |
| (다른 PC) 휴대폰에서 `431 Header fields are too long` | `esp_http_server` 헤더 길이 제한(1024B)이 코어에 고정 | 요청 첫 줄만 읽고 헤더는 길이와 상관없이 건너뜀 |
| 휴대폰에서 보드 Wi-Fi(AP)가 안 보임 | AP 펌웨어를 컴파일만 하고 **업로드 전**이었다 | 업로드 후 PC에서 Wi-Fi 강제 재검색으로 신호 99% 확인 |
| `arduino-cli monitor`에 `timeout`을 걸면 로그가 비어 나옴 | Git Bash 환경 문제 | PowerShell `System.IO.Ports.SerialPort`로 읽기 |
| (MQTT) 보드가 PC의 Mosquitto에 접속 못 함 | Windows `mosquitto` 서비스는 설정 파일 없이 돌아서 **localhost:1883만** 연다 | 프로젝트 설정 파일로 브로커를 따로 실행 (MQTT 1884, WebSocket 9001) |
| (MQTT) 어제 되던 보드가 오늘 접속 실패 | 공유기가 **PC IP를 바꿨다** (.27 → .22). 보드에는 예전 IP가 들어 있었다 | `secrets.h`의 `MQTT_HOST` 수정 후 재업로드. 공유기에서 PC IP 고정(DHCP 예약) 권장 |
| (MQTT) LED가 한 박자 늦고 터치 메시지가 5초에 6개만 옴 | 아무도 COM 포트를 안 읽으면 `Serial.printf`가 **몇 초씩 막혀** `loop()`가 멈춤. 시리얼 창을 열고 테스트하면 증상이 사라져서 늦게 찾음 | `Serial.setTxTimeoutMs(0)`로 출력을 버리게 함 → 초당 5개, LED 1초 안에 반영. 함께 `WiFi.setSleep(false)` |
| (EI) 사진을 지웠는데 휴지통에 복사본이 남고 원본도 그대로 | Windows에서 브라우저가 읽는 중인 파일을 `shutil.move`하면 복사로 바뀌고 원본 삭제는 실패 | `os.replace` 재시도만 쓰고, 실패한 장수는 화면에 알림 |
| (EI) MobileNetV2가 float32 100%인데 보드용 int8은 76% | 양자화로 정확도가 크게 떨어짐 | Dense 층 제거(88%) → MobileNetV1 0.25로 교체(int8 = float32 = 90.9%) |
| (EI) 원격 대시보드가 "오프라인" | 수집 펌웨어(`EICamera`)가 운영 펌웨어를 덮어써서 클라우드 접속이 멈춤 | `SupabaseDashboard` 재업로드. 펌웨어는 한 번에 하나만 올라간다 |
| (EI) 업로드 후 `ERR: Failed to allocate persistent buffer ... EI_MAX_OVERFLOW_BUFFER_COUNT` → 재부팅 반복 | ESP-NN 가속 커널이 층마다 버퍼를 따로 요청(20~40KB짜리 포함 수십 개). 라이브러리는 텐서 아레나 밖 추가 할당을 ESP32-S3에서 **30개**까지만 허용 | `ei_classifier_porting.h`의 `EI_MAX_OVERFLOW_BUFFER_COUNT`를 100으로. 아레나만 64KB 늘려서는 해결되지 않았다 |
| (EI) 그다음 `CORRUPT HEAP: Bad head ...` → 재부팅 반복 | 추가 버퍼를 `calloc`으로 받아 **16바이트 정렬이 안 됨**. S3 SIMD 명령이 정렬된 주소로 내려 써서 버퍼 앞의 힙 헤더를 덮어씀 | 모델 `.cpp`에서 `ei_calloc` 대신 정렬된 `ei_malloc`(`heap_caps_aligned_alloc(16, …)`) + `memset`. 방법은 [EIInference/README](EIInference/README.md) |
| (MQTT) 대시보드 페이지가 갑자기 안 열림 | 브로커·웹 서버를 띄운 창이 닫히거나 메모리 부족으로 종료 | `start-all.ps1`로 다시 실행하고 두 창을 열어 둔다 |

---

## 스케치

| 폴더 | 설명 | 빌드 옵션 |
| --- | --- | --- |
| [Blink](Blink/) | LED 켜기·끄기를 `delay` 없이 반복 → 너무 빨라서 눈에는 계속 켜진 것처럼 보인다 | 기본 |
| [TouchLED](TouchLED/) | D0을 터치하는 동안 LED 켜기, 시리얼로 기준값·현재값 출력 | 기본 |
| [WebDashboard](WebDashboard/) | 같은 Wi-Fi 웹 대시보드 (LED, 터치) | 기본 |
| [SupabaseDashboard](SupabaseDashboard/) | 로컬 대시보드 + 카메라(80/81번 포트) + Supabase 원격 동기화. `dashboard.html`은 GitHub Pages용 원격 대시보드 | `PSRAM=opi` |
| [APDashboard](APDashboard/) | AP 모드 대시보드 (`192.168.4.1`) | 기본 |
| [BLEDashboard](BLEDashboard/) | BLE 서버 + 휴대폰용 Web Bluetooth 페이지(`ble.html`) | 기본 |
| [MqttDashboard](MqttDashboard/) | MQTT 펌웨어 + 브로커 설정 + 웹 대시보드(`web/index.html`). 실행법·토픽은 [폴더 README](MqttDashboard/README.md) | 기본 |
| [EICamera](EICamera/) | Edge Impulse 사진 수집용 카메라 펌웨어 (240×240 시작, 해상도·품질 API) | `PSRAM=opi` |
| [EICollector](EICollector/) | PC 수집 대시보드 (파이썬 표준 라이브러리만). [폴더 README](EICollector/README.md) | – |
| [EIModel](EIModel/) | 학습 스크립트와 Arduino 라이브러리 zip, 결과. [폴더 README](EIModel/README.md) | – |
| [EIInference](EIInference/) | 보드에서 분류 + 결과 페이지 (컴파일 확인 전). [폴더 README](EIInference/README.md) | `PSRAM=opi` |
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

MQTT는 PC에 [Mosquitto](https://mosquitto.org/download/)를 설치하고 `MqttDashboard\start-all.ps1`(브로커 + 웹 서버)을 실행한 뒤 `MqttDashboard`를 업로드합니다. `secrets.h`의 `MQTT_HOST`는 PC의 Wi-Fi IP입니다.

Supabase를 쓰려면 [supabase/xiao_schema.sql](supabase/xiao_schema.sql)을 SQL 편집기에서 실행하고, `<DEVICE_SECRET>` 자리와 `secrets.h`에 같은 값을 넣습니다.

> 부팅하는 몇 초 동안은 D0 터치센서를 만지지 마세요. 이때 기준값을 잽니다.

---

## 폴더 구조

```text
xiao-esp32s3-lab/
├── Blink/ · TouchLED/          기초 예제
├── WebDashboard/               같은 Wi-Fi 웹서버
├── SupabaseDashboard/          원격 제어 + 카메라
│   ├── camera_stream.h         카메라 서버 (81번 포트)
│   ├── dashboard_html.h        보드가 보여 주는 페이지
│   └── dashboard.html          GitHub Pages 원격 대시보드
├── APDashboard/                AP 모드
├── BLEDashboard/               BLE + ble.html
├── MqttDashboard/              MQTT
│   ├── broker/mosquitto.conf   로컬 브로커 설정 (1884 / 9001)
│   ├── web/index.html          웹 대시보드 (mqtt.js)
│   └── start-all.ps1           브로커 + 웹 서버 한 번에 실행
├── EICamera/                   사진 수집용 카메라 펌웨어
├── EICollector/                PC 수집 대시보드 (server.py, web/index.html)
├── EIModel/                    Edge Impulse 학습 스크립트 + Arduino 라이브러리 zip
├── EIInference/                보드에서 분류
├── supabase/xiao_schema.sql    DB 구성
├── AGENTS.md                   AI 에이전트 작업 규칙 (실제로 겪은 문제 정리)
└── STUDY_NOTES.md              학습 정리
```

## 참고

- 보드 공식 문서: [Seeed Studio XIAO ESP32-S3 위키](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- 원격 대시보드 배포 저장소: [xiao-dashboard](https://github.com/leesm3528-netizen/xiao-dashboard)
