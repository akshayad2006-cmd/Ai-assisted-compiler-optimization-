from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field

ROOT = Path(__file__).resolve().parents[2]
COMPILER = ROOT / "build" / "aicompiler"
AI_PIPELINE = ROOT / "python" / "optimize_with_ai.py"
EXAMPLES_DIR = ROOT / "examples"

STAGES = {
    "tokens": ["--tokens"],
    "parse": ["--parse"],
    "ast": ["--ast"],
    "check": ["--check"],
    "symbols": ["--symbols"],
    "tac": ["--tac"],
    "optimize": ["--optimize"],
    "opt_report": ["--opt-report"],
    "analyze": ["--analyze"],
    "performance": ["--performance"],
    "llvm": ["--llvm"],
}

app = FastAPI(
    title="AI-Assisted Compiler Optimization System",
    version="1.0.0",
    description="Web API for the existing C++ compiler and Python recommender.",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173", "http://127.0.0.1:5173"],
    allow_credentials=False,
    allow_methods=["GET", "POST"],
    allow_headers=["Content-Type"],
)


class SourceRequest(BaseModel):
    source: str = Field(min_length=1, max_length=50000)


class AIRequest(BaseModel):
    source: str = Field(min_length=1, max_length=50000)
    provider: str = "fake:greedy"


def run_command(command: list[str], timeout: int = 15) -> dict:
    try:
        result = subprocess.run(
            command,
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False,
            env={**os.environ, "PYTHONUNBUFFERED": "1"},
        )
    except subprocess.TimeoutExpired:
        return {
            "ok": False,
            "stdout": "",
            "stderr": f"Command timed out after {timeout} seconds.",
            "returncode": -1,
        }
    except OSError as exc:
        return {
            "ok": False,
            "stdout": "",
            "stderr": str(exc),
            "returncode": -1,
        }

    return {
        "ok": result.returncode == 0,
        "stdout": result.stdout,
        "stderr": result.stderr,
        "returncode": result.returncode,
    }


def check_setup() -> None:
    if not COMPILER.is_file():
        raise HTTPException(
            status_code=503,
            detail="Compiler executable not found. Run: bash scripts/build.sh",
        )


@app.get("/api/health")
def health():
    return {
        "status": "ok",
        "compiler_exists": COMPILER.is_file(),
        "ai_pipeline_exists": AI_PIPELINE.is_file(),
    }


@app.get("/api/examples")
def examples():
    return [
        {"name": path.name, "source": path.read_text(encoding="utf-8")}
        for path in sorted(EXAMPLES_DIR.glob("*.src"))
        if path.is_file()
    ]


@app.post("/api/run/{stage}")
def run_stage(stage: str, request: SourceRequest):
    check_setup()

    if stage not in STAGES:
        raise HTTPException(status_code=404, detail=f"Unknown compiler stage: {stage}")

    with tempfile.TemporaryDirectory(prefix="aicompiler_web_") as temp:
        source_file = Path(temp) / "program.src"
        source_file.write_text(request.source, encoding="utf-8")

        result = run_command([
            str(COMPILER),
            *STAGES[stage],
            str(source_file),
        ])

    if stage == "analyze" and result["ok"]:
        try:
            result["data"] = json.loads(result["stdout"])
        except json.JSONDecodeError:
            result["ok"] = False
            result["stderr"] = "Compiler analysis did not return valid JSON."

    return result


@app.post("/api/ai")
def run_ai(request: AIRequest):
    check_setup()

    if not AI_PIPELINE.is_file():
        raise HTTPException(status_code=503, detail="AI pipeline script not found.")

    # The UI is intentionally limited to the project's deterministic fake
    # providers for this local demonstration. No arbitrary provider commands.
    if request.provider not in {"fake:greedy", "fake:none"}:
        raise HTTPException(
            status_code=400,
            detail="Choose fake:greedy or fake:none for the local demo.",
        )

    with tempfile.TemporaryDirectory(prefix="aicompiler_ai_web_") as temp:
        source_file = Path(temp) / "program.src"
        source_file.write_text(request.source, encoding="utf-8")

        result = run_command([
            os.fspath(Path(os.sys.executable)),
            str(AI_PIPELINE),
            "--source",
            str(source_file),
            "--compiler",
            str(COMPILER),
            "--provider",
            request.provider,
            "--max-attempts",
            "2",
        ], timeout=30)

    try:
        result["data"] = json.loads(result["stdout"])
    except json.JSONDecodeError:
        result["data"] = None

    return result
