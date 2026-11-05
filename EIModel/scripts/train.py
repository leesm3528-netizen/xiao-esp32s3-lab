"""Create the impulse, generate features, train, and build the Arduino library.

Run after upload.py:  python EIModel/scripts/train.py
Settings are the ones that gave the v1 model (MobileNetV1 0.25, int8 = float32 accuracy).
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from ei import req  # noqa: E402
from job import P, wait  # noqa: E402

OUT = pathlib.Path(__file__).resolve().parents[1]

IMPULSE = {
    "inputBlocks": [{"id": 1, "type": "image", "name": "Image", "title": "Image data",
                     "imageWidth": 96, "imageHeight": 96, "resizeMode": "fit-short"}],
    "dspBlocks": [{"id": 2, "type": "image", "name": "Image", "axes": ["image"], "title": "Image", "input": 1}],
    "learnBlocks": [{"id": 3, "type": "keras-transfer-image", "name": "Transfer learning", "dsp": [2],
                     "title": "Transfer Learning (Images)"}],
}

TRAIN = {
    "mode": "visual",
    "trainingCycles": 40,
    "learningRate": 0.0005,
    "trainTestSplit": 0.2,
    # MobileNetV2 0.35 reached 100% float32 but only 76% (with dense 16) / 88% (no dense) as int8.
    # MobileNetV1 0.25 keeps int8 == float32, and is lighter on the ESP32-S3.
    "visualLayers": [{"type": "transfer_mobilenetv1_a25_d100", "neurons": 0, "dropoutRate": 0.1}],
    "augmentationPolicyImage": "all",
    "profileInt8": True,
    "selectedModelType": "int8",
}


def main():
    if req("GET", f"/{P}/impulse").get("impulse") is None:
        print("impulse:", req("POST", f"/{P}/impulse", IMPULSE))
    req("POST", f"/{P}/dsp/2", {"config": {"channels": "RGB"}})

    r = req("POST", f"/{P}/jobs/generate-features", {"dspId": 2})
    if not wait(r["id"], "generate-features"):
        sys.exit(1)

    r = req("POST", f"/{P}/jobs/train/keras/3", TRAIN)
    if not wait(r["id"], "train"):
        sys.exit(1)
    for m in req("GET", f"/{P}/training/keras/3/metadata").get("modelValidationMetrics", []):
        print(m.get("type"), "validation accuracy", m.get("accuracy"), "confusion", m.get("confusionMatrix"))

    r = req("POST", f"/{P}/jobs/build-ondevice-model?type=arduino", {"engine": "tflite-eon", "modelType": "int8"})
    if not wait(r["id"], "build-arduino"):
        sys.exit(1)
    data = req("GET", f"/{P}/deployment/download?type=arduino&modelType=int8&engine=tflite-eon", raw=True)
    out = OUT / f"ei-mint-waterbottle-arduino-v{r.get('deploymentVersion', 'x')}.zip"
    out.write_bytes(data)
    print("saved", out, len(data), "bytes")


if __name__ == "__main__":
    main()
