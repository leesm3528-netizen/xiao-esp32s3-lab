import sys, time, json
import pathlib; sys.path.insert(0, str(pathlib.Path(__file__).parent)); from ei import req
P = 1156909

def wait(job_id, label, limit=1800):
    t0 = time.time()
    while time.time() - t0 < limit:
        j = req("GET", f"/{P}/jobs/{job_id}/status")["job"]
        if j.get("finished"):
            ok = j.get("finishedSuccessful")
            print(f"{label}: job {job_id} {'OK' if ok else 'FAILED'} in {time.time()-t0:.0f}s")
            if not ok:
                out = req("GET", f"/{P}/jobs/{job_id}/stdout")
                print("\n".join(s["data"].rstrip() for s in reversed(out.get("stdout", [])))[-3000:])
            return ok
        time.sleep(5)
    print(label, "timeout"); return False
