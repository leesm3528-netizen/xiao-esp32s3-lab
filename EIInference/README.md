# EIInference — 보드에서 민트 / 물병 분류

`EIModel`에서 학습한 Edge Impulse 모델을 XIAO ESP32-S3 Sense에서 돌리는 펌웨어다.
카메라 영상을 보면서 분류 결과를 웹 페이지와 시리얼로 확인한다.

```text
카메라 JPEG 240×240 ─┬─> :81/stream, /capture (영상, camera_stream.h)
                     └─> 추론 작업(코어 1): JPEG → RGB888 → 96×96 → run_classifier()
                             └─> /api/result, 시리얼, LED
```

## 준비 (처음 한 번)

Edge Impulse 라이브러리를 Arduino 라이브러리 폴더에 설치한다.

```bash
# 방법 1: zip을 풀어 C:\Users\<사용자>\Documents\Arduino\libraries\ 에 복사 (이 PC는 이렇게 설치함)
# 방법 2: arduino-cli 설정을 바꾸고 설치
arduino-cli config set library.enable_unsafe_install true
arduino-cli lib install --zip-path EIModel/ei-mint-waterbottle-arduino-v1.zip
```

**설치 후 라이브러리 두 곳을 고쳐야 한다.** 그대로 두면 부팅 직후 오류로 재부팅을 반복한다.

1. `src/edge-impulse-sdk/porting/ei_classifier_porting.h`에서 `CONFIG_IDF_TARGET_ESP32S3`의 `EI_MAX_OVERFLOW_BUFFER_COUNT`를 `30`에서 `100`으로 바꾼다. ESP-NN이 층마다 버퍼를 따로 받아서 30개를 넘긴다(`ERR: Failed to allocate persistent buffer ... EI_MAX_OVERFLOW_BUFFER_COUNT`).
2. `src/tflite-model/tflite_learn_1156909_3_compiled.cpp`의 `AllocatePersistentBufferImpl`에서 `ptr = ei_calloc(bytes, 1);`을 `ptr = ei_malloc(bytes);`로 바꾸고, NULL 검사 뒤에 `memset(ptr, 0, bytes);`를 넣는다. S3 SIMD는 16바이트 정렬이 필요한데 `calloc`은 정렬되지 않아 `CORRUPT HEAP`이 났다.

`secrets.h`에 와이파이 정보를 넣는다(`secrets.example.h` 참고, gitignore).

## 빌드·업로드

```bash
FQBN=esp32:esp32:XIAO_ESP32S3:PSRAM=opi
arduino-cli compile --fqbn $FQBN --jobs 2 EIInference   # 처음에는 SDK 전체를 컴파일해서 오래 걸린다
arduino-cli upload -p COM4 --fqbn $FQBN EIInference
```

- `--jobs 2`: 메모리가 부족한 PC에서 동시 컴파일 수를 줄인다.
- 올리면 운영 펌웨어(`SupabaseDashboard`)를 덮어쓴다. 외부 대시보드를 다시 쓰려면 그것을 다시 올릴 것.

## 사용

| 무엇 | 어디 |
| --- | --- |
| 영상 + 결과 페이지 | <http://192.168.0.25/> (같은 와이파이) |
| 결과 JSON | `/api/result` → `label`, `score`, `scores`, `dsp_ms`, `nn_ms`, `total_ms`, `count`, `led` |
| 영상 | `:81/stream`, `:81/capture` |
| 시리얼 | `#12 WaterBottle 0.97 \| Mint 0.03 WaterBottle 0.97 \| dsp 3 nn 60 total 95 ms` |
| LED | `LED_LABEL`(기본 `WaterBottle`)이 `LED_THRESHOLD`(0.80) 이상이면 켜짐 |

설정은 `EIInference.ino` 맨 위의 `LED_LABEL`, `LED_THRESHOLD`, `INFER_INTERVAL_MS`(기본 200ms)다.

## 알아둘 점

- 카메라는 학습 사진과 같은 240×240 JPEG(품질 12)로 둔다. 다르게 찍으면 정확도가 떨어질 수 있다.
- `background` 클래스가 없다. 빈 화면이나 다른 물건도 민트나 물병 중 하나로 판정한다.
- 학습 데이터와 비슷한 거리·배경·조명에서 가장 잘 맞는다.
- 영상을 계속 보면 보드가 뜨거워진다. 오래 쓸 때는 방열판을 붙인다.
