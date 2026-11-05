# EIModel — 민트 / 물병 분류 모델 (Edge Impulse)

`EICollector`로 모은 사진으로 Edge Impulse에서 학습한 이미지 분류 모델과 학습 스크립트다.

## 결과 (v1, 2026-11-04)

| 항목 | 값 |
| --- | --- |
| Edge Impulse 프로젝트 | `leesm3528-netizen-project-1` (ID 1156909) |
| 클래스 | `Mint`, `WaterBottle` |
| 데이터 | 학습 161장 / 테스트 40장 (클래스별 80:20, seed 42) |
| 입력 | 96×96 RGB (240×240 사진을 줄임) |
| 모델 | MobileNetV1 0.25 전이학습, 출력층 바로 연결, 증강 사용, 40 epoch |
| 양자화 | int8 (ESP32-S3에서 빠름) |
| 검증 정확도 | int8 90.9% (float32와 같음) |
| **테스트 정확도** | **90% (40장 중 36장)**, Mint 18/20, WaterBottle 18/20 |
| 배포 파일 | `ei-mint-waterbottle-arduino-v1.zip` (Arduino 라이브러리 `leesm3528-netizen-project-1_inferencing`) |

### 모델을 고른 과정

| 시도 | float32 검증 | int8 검증 |
| --- | --- | --- |
| MobileNetV2 0.35 + Dense 16 | 100% | 75.8% |
| MobileNetV2 0.35, Dense 없음 | 100% | 87.9% |
| **MobileNetV1 0.25, Dense 없음** | 90.9% | **90.9%** |

MobileNetV2는 float32로는 완벽했지만 int8로 바꾸면 정확도가 크게 떨어졌다. 보드에서는 int8을 쓰기 때문에 int8 점수가 가장 높은 MobileNetV1을 골랐다.

### 틀린 테스트 사진

- 크게 흔들려 형체가 안 보이는 사진(물병 2장, 민트 1장)
- 민트 사진인데 뒤에 물병이 같이 찍힌 사진

모델보다 데이터 문제다. 정확도를 더 올리려면:

1. `EICollector` 대시보드에서 흔들린 사진을 지운다.
2. 한 사진에 물체 하나만 나오게 다시 찍는다.
3. 물체가 없는 `background` 클래스를 추가한다(없으면 빈 화면도 민트나 물병으로 판정한다).
4. 아래 스크립트로 다시 올리고 학습한다.

## 다시 학습하기

`.env`의 `EI_API_KEY`(프로젝트 API 키)를 쓴다. 키는 출력하지 않는다.

```bash
# 주의: upload.py는 지금 dataset을 그대로 추가로 올린다. 다시 올릴 때는 Edge Impulse의
# Data acquisition에서 기존 데이터를 먼저 지울 것(같은 사진이 두 번 들어가면 점수가 부풀려진다).
python EIModel/scripts/upload.py     # 클래스별 80:20으로 training / testing 업로드
python EIModel/scripts/train.py      # impulse → 특징 추출 → 학습 → Arduino 라이브러리 저장
python EIModel/scripts/evaluate.py   # 테스트 데이터로 평가
```

| 파일 | 역할 |
| --- | --- |
| `scripts/ei.py` | REST 호출 도우미(`x-api-key` 헤더) |
| `scripts/job.py` | 작업 완료 대기, 실패하면 로그 출력 |
| `scripts/upload.py` | ingestion API로 사진 업로드 |
| `scripts/train.py` | 학습 설정(v1과 같음)과 배포 파일 빌드 |
| `scripts/evaluate.py` | 테스트 데이터 분류 결과·혼동표 |

## 다음 단계: 보드에서 추론

```bash
arduino-cli config set library.enable_unsafe_install true
arduino-cli lib install --zip-path EIModel/ei-mint-waterbottle-arduino-v1.zip
```

라이브러리의 `examples/esp32/esp32_camera`가 출발점이다. XIAO 카메라 핀(`EICamera/camera_stream.h`)과 PSRAM 설정(`PSRAM=opi`)으로 바꿔야 한다.
