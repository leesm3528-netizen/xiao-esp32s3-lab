import sys, json, collections
import pathlib; sys.path.insert(0, str(pathlib.Path(__file__).parent)); from ei import req; from job import wait
P = 1156909
mt = sys.argv[1] if len(sys.argv) > 1 else "int8"
try:
    r = req("POST", f"/{P}/jobs/classify", {"modelVariants": [mt]})
except Exception:
    r = req("POST", f"/{P}/jobs/classify", {})
print(r); wait(r["id"], "classify")
res = req("GET", f"/{P}/classify/all/result?featureExplorerOnly=false&variant={mt}")
acc = res.get("accuracy", {})
print("summary:", json.dumps({k: acc.get(k) for k in ("totalSummary", "summaryPerClass", "accuracyScore", "mseScore")}, ensure_ascii=False)[:800])
cm = collections.Counter()
for s in res.get("result", []):
    exp = s["sample"]["label"]
    for c in s.get("classifications", []):
        for w in c.get("result", []):
            best = max(w.items(), key=lambda kv: kv[1])
            got = best[0] if best[1] >= 0.6 else "uncertain"
            cm[(exp, got)] += 1
print("per-window (threshold 0.6):", dict(cm))
