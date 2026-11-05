import sys, random, uuid, pathlib, json
sys.path.insert(0, str(pathlib.Path(__file__).parent)); from ei import req
DATA = pathlib.Path(__file__).resolve().parents[2] / "EICollector" / "dataset"
ING = "https://ingestion.edgeimpulse.com"

def upload(category, label, files):
    for i in range(0, len(files), 20):  # batches of 20
        b = uuid.uuid4().hex
        body = b""
        for f in files[i:i+20]:
            body += (f"--{b}\r\nContent-Disposition: form-data; name=\"data\"; filename=\"{f.name}\"\r\n"
                     f"Content-Type: image/jpeg\r\n\r\n").encode() + f.read_bytes() + b"\r\n"
        body += f"--{b}--\r\n".encode()
        r = req("POST", f"/api/{category}/files", base=ING, data=body,
                headers={"Content-Type": f"multipart/form-data; boundary={b}", "x-label": label})
        bad = [x for x in r.get("files", []) if not x.get("success")]
        print(category, label, i, "ok" if r.get("success") and not bad else r)

random.seed(42)
for d in sorted(p for p in DATA.iterdir() if p.is_dir() and not p.name.startswith('.')):
    files = sorted(d.glob("*.jpg")); random.shuffle(files)
    n_test = round(len(files) * 0.2)
    upload("testing", d.name, files[:n_test])
    upload("training", d.name, files[n_test:])
    print(d.name, "train", len(files) - n_test, "test", n_test)
