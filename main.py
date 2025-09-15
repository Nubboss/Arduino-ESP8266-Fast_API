# main.py
from fastapi import FastAPI, Request, HTTPException
from fastapi.responses import HTMLResponse, FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
from typing import Optional, List
from pathlib import Path
import json
from datetime import datetime

DATA_DIR = Path(__file__).parent / "data"
DATA_DIR.mkdir(exist_ok=True)
DATA_FILE = DATA_DIR / "data.json"

# load or init storage
if DATA_FILE.exists():
    with open(DATA_FILE, "r", encoding="utf-8") as f:
        try:
            STORAGE = json.load(f)
        except Exception:
            STORAGE = []
else:
    STORAGE = []

app = FastAPI(title="Test Sensors")

# serve static UI
static_dir = Path(__file__).parent / "static"
app.mount("/static", StaticFiles(directory=static_dir), name="static")
class SensorPayload(BaseModel):
    device: str
    temperature: Optional[float] = None
    humidity: Optional[float] = None

def persist():
    with open(DATA_FILE, "w", encoding="utf-8") as f:
        json.dump(STORAGE, f, ensure_ascii=False, indent=2)

@app.post("/send")
async def ingest(payload: SensorPayload):
    if payload.temperature is None and payload.humidity is None:
        raise HTTPException(status_code=400, detail="No Data!")

    record = {
        "device": payload.device,
        "temperature": payload.temperature,
        "humidity": payload.humidity,
        "received_at": datetime.utcnow().isoformat()
    }
    STORAGE.append(record)
   
    if len(STORAGE) > 10000:
        STORAGE[:] = STORAGE[-5000:]
        print(">10000")
    persist()
    return {"status": "ok"}

@app.get("/data", response_class=JSONResponse)
async def get_data(limit: int = 500):
    # return last `limit` entries (most recent last)
    return STORAGE[-limit:]

@app.get("/", response_class=HTMLResponse)
async def index():
    return FileResponse(Path(__file__).parent / "static" / "index.html")
