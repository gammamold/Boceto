"""
yt-dlp HTTP server.
POST /fetch  { "url": "...", "sample_rate": 44100 }  ->  audio/wav
GET  /health
Run:  uvicorn main:app --host 0.0.0.0 --port 8000
Requires: pip install -r requirements.txt   AND   ffmpeg on PATH
"""
import hashlib
import os
import subprocess
import sys
import tempfile

from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse
from pydantic import BaseModel

app = FastAPI()

CACHE_DIR = os.environ.get("CACHE_DIR", "/tmp/yt_sampler_cache")
os.makedirs(CACHE_DIR, exist_ok=True)


class FetchRequest(BaseModel):
    url: str
    sample_rate: int = 44100


def cache_path(url: str, sr: int) -> str:
    h = hashlib.sha256(f"{url}|{sr}".encode()).hexdigest()[:16]
    return os.path.join(CACHE_DIR, f"{h}.wav")


@app.post("/fetch")
def fetch(req: FetchRequest):
    out = cache_path(req.url, req.sample_rate)
    if os.path.exists(out):
        return FileResponse(out, media_type="audio/wav", filename="audio.wav")

    tmp_dir = tempfile.mkdtemp(prefix="ytsamp_")
    try:
        cmd = [
            sys.executable, "-m", "yt_dlp",
            "-x", "--audio-format", "wav",
            "--postprocessor-args", f"-ar {req.sample_rate} -ac 2",
            "-o", os.path.join(tmp_dir, "audio.%(ext)s"),
            "--no-playlist",
            req.url,
        ]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
        if result.returncode != 0:
            raise HTTPException(status_code=500, detail=result.stderr[-800:])

        wav = next((f for f in os.listdir(tmp_dir) if f.endswith(".wav")), None)
        if wav is None:
            raise HTTPException(status_code=500, detail="no wav produced")

        os.replace(os.path.join(tmp_dir, wav), out)
        return FileResponse(out, media_type="audio/wav", filename="audio.wav")
    finally:
        for f in os.listdir(tmp_dir):
            try:
                os.remove(os.path.join(tmp_dir, f))
            except OSError:
                pass
        try:
            os.rmdir(tmp_dir)
        except OSError:
            pass


@app.get("/health")
def health():
    return {"ok": True}
