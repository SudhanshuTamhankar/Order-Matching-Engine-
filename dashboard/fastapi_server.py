import asyncio
import json
import os
import subprocess
from pathlib import Path
from typing import List

try:
    from fastapi import FastAPI, WebSocket, WebSocketDisconnect
    from fastapi.middleware.cors import CORSMiddleware
    from fastapi.responses import FileResponse, JSONResponse
    from fastapi.staticfiles import StaticFiles
    import uvicorn
    FASTAPI_AVAILABLE = True
except ImportError:
    FASTAPI_AVAILABLE = False

# Path configurations
BASE_DIR = Path(__file__).resolve().parent
RESULTS_DIR = BASE_DIR.parent / "results"
TRADES_FILE = RESULTS_DIR / "trades.json"
SNAPSHOTS_FILE = RESULTS_DIR / "book_snapshots.jsonl"

if FASTAPI_AVAILABLE:
    app = FastAPI(
        title="ApexMatch Engine Gateway",
        description="FastAPI & WebSocket streaming service for Low-Latency Order Matching Engine",
        version="2.0.0"
    )

    app.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    class ConnectionManager:
        def __init__(self):
            self.active_connections: List[WebSocket] = []

        async def connect(self, websocket: WebSocket):
            await websocket.accept()
            self.active_connections.append(websocket)

        def disconnect(self, websocket: WebSocket):
            if websocket in self.active_connections:
                self.active_connections.remove(websocket)

        async def broadcast(self, message: dict):
            for connection in list(self.active_connections):
                try:
                    await connection.send_json(message)
                except Exception:
                    self.disconnect(connection)

    manager = ConnectionManager()

    @app.get("/")
    async def serve_index():
        index_path = BASE_DIR / "interactive_visualizer.html"
        if not index_path.exists():
            index_path = BASE_DIR / "index.html"
        return FileResponse(index_path)

    @app.get("/get_trades")
    @app.get("/api/trades")
    async def get_trades():
        if not TRADES_FILE.exists():
            return JSONResponse([])
        try:
            return FileResponse(TRADES_FILE, media_type="application/json")
        except Exception as e:
            return JSONResponse({"error": str(e)}, status_code=500)

    @app.get("/get_snapshots")
    @app.get("/api/snapshots")
    async def get_snapshots():
        if not SNAPSHOTS_FILE.exists():
            return JSONResponse([])
        try:
            snapshots = []
            with open(SNAPSHOTS_FILE, "r", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if line:
                        snapshots.append(json.loads(line))
            return JSONResponse(snapshots)
        except Exception as e:
            return JSONResponse({"error": str(e)}, status_code=500)

    @app.get("/api/status")
    async def get_status():
        return {
            "status": "online",
            "trades_exist": TRADES_FILE.exists(),
            "snapshots_exist": SNAPSHOTS_FILE.exists(),
            "trades_size_bytes": TRADES_FILE.stat().st_size if TRADES_FILE.exists() else 0,
            "snapshots_size_bytes": SNAPSHOTS_FILE.stat().st_size if SNAPSHOTS_FILE.exists() else 0
        }

    @app.websocket("/ws/stream")
    async def websocket_stream(websocket: WebSocket):
        await manager.connect(websocket)
        try:
            # Stream trades in chunks to connected client
            if TRADES_FILE.exists():
                with open(TRADES_FILE, "r", encoding="utf-8") as f:
                    trades = json.load(f)
                
                chunk_size = 200
                for i in range(0, len(trades), chunk_size):
                    chunk = trades[i:i + chunk_size]
                    await websocket.send_json({
                        "type": "trade_batch",
                        "trades": chunk,
                        "progress": min(1.0, (i + chunk_size) / len(trades))
                    })
                    await asyncio.sleep(0.01)
            
            while True:
                data = await websocket.receive_text()
                await websocket.send_json({"type": "pong", "message": data})
        except WebSocketDisconnect:
            manager.disconnect(websocket)
        except Exception:
            manager.disconnect(websocket)

    # Mount static assets
    app.mount("/", StaticFiles(directory=str(BASE_DIR), html=True), name="static")

if __name__ == "__main__":
    if FASTAPI_AVAILABLE:
        print("Starting FastAPI ASGI Gateway on http://127.0.0.1:8000 ...")
        uvicorn.run("fastapi_server:app", host="0.0.0.0", port=8000, reload=True)
    else:
        print("FastAPI / Uvicorn not installed. Please run: pip install fastapi uvicorn")
