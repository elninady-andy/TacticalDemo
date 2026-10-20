"""Small OpenAI-compatible HTTP server for a local Transformers Qwen model."""

from __future__ import annotations

import argparse
import json
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


def normalize_local_identity(text: str) -> str:
    """Prevent the small model from falsely presenting this local runtime as cloud-hosted."""
    if any(marker in text.casefold() for marker in ("云端服务", "云服务", "阿里云", "cloud service")):
        return "我是本机部署的 Qwen UE场景助手，当前通过本地模型服务回答你的问题。"
    return text


def safe_local_answer(text: str) -> str:
    """Bound plain-text fallback and prevent executable content from becoming chat text."""
    clean = normalize_local_identity(text.strip())
    folded = clean.casefold()
    if not clean or any(marker in folded for marker in ("```", "sub catmain", "import ", "exec(", "eval(", "powershell")):
        return "我已收到你的问题，请用自然语言补充需要了解或编辑的场景。"
    return clean[:800]


class QwenRuntime:
    def __init__(self, model_dir: str | Path) -> None:
        import torch
        from transformers import AutoModelForCausalLM, AutoTokenizer

        self.torch = torch
        torch.set_num_threads(max(1, min(8, torch.get_num_threads())))
        torch.set_num_interop_threads(1)
        self.tokenizer = AutoTokenizer.from_pretrained(str(model_dir), local_files_only=True)
        # The model metadata requests bfloat16, but the local 11th-gen Intel CPU has
        # no native BF16 arithmetic. BF16 emulation made a tiny reply take >120s.
        # Float32 uses more RAM but is substantially faster and stable on this host.
        self.model = AutoModelForCausalLM.from_pretrained(
            str(model_dir), local_files_only=True, dtype=torch.float32
        ).to("cpu")
        self.model.eval()
        self.generation_lock = threading.Lock()

    def _generate(self, messages: list[dict[str, str]], max_tokens: int) -> str:
        prompt = self.tokenizer.apply_chat_template(messages, tokenize=False, add_generation_prompt=True)
        inputs = self.tokenizer([prompt], return_tensors="pt")
        with self.torch.inference_mode():
            output = self.model.generate(
                **inputs,
                max_new_tokens=max(32, min(int(max_tokens), 1024)),
                do_sample=False,
                repetition_penalty=1.05,
            )
        generated = output[0][inputs.input_ids.shape[1] :]
        return self.tokenizer.decode(generated, skip_special_tokens=True).strip()

    def chat(self, messages: list[dict[str, str]], max_tokens: int = 160) -> str:
        # Transformers generation on one model instance is not thread-safe. Queue a
        # second UI click instead of running two generations against shared caches.
        with self.generation_lock:
            answer = self._generate(messages, max_tokens)
        system = next((item.get("content", "") for item in messages if item.get("role") == "system"), "")
        if not system.startswith("Return exactly one JSON object"):
            return answer
        try:
            payload = json.loads(answer.removeprefix("```json").removesuffix("```").strip())
            if isinstance(payload, dict) and payload.get("kind") in {"message", "clarification", "part_proposal"}:
                if payload.get("kind") == "message" and isinstance(payload.get("text"), str):
                    payload["text"] = normalize_local_identity(payload["text"])
                return json.dumps(payload, ensure_ascii=False)
        except (json.JSONDecodeError, AttributeError):
            pass
        return json.dumps(
            {"kind": "message", "text": safe_local_answer(answer), "parameters": {}, "suggestions": []},
            ensure_ascii=False,
        )


def make_handler(runtime: QwenRuntime):
    class Handler(BaseHTTPRequestHandler):
        def _json(self, status: int, payload: object) -> None:
            body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            try:
                self.wfile.write(body)
            except (BrokenPipeError, ConnectionAbortedError):
                # The caller may cancel a long CPU generation; the server remains healthy.
                return

        def do_GET(self) -> None:  # noqa: N802
            if self.path.rstrip("/") == "/v1/models":
                self._json(200, {"object": "list", "data": [{"id": "qwen", "object": "model"}]})
            elif self.path.rstrip("/") == "/health":
                self._json(200, {"status": "ok", "model": "qwen"})
            else:
                self._json(404, {"error": "not found"})

        def do_POST(self) -> None:  # noqa: N802
            if self.path.rstrip("/") != "/v1/chat/completions":
                self._json(404, {"error": "not found"})
                return
            try:
                length = int(self.headers.get("Content-Length", "0"))
                payload = json.loads(self.rfile.read(length).decode("utf-8"))
                messages = payload.get("messages")
                if not isinstance(messages, list):
                    raise ValueError("messages required")
                if payload.get("model") not in (None, "qwen"):
                    raise ValueError("model must be qwen")
                content = runtime.chat(messages, min(payload.get("max_tokens", 160), 1024))
                self._json(200, {
                    "id": "tactical-demo-qwen-local",
                    "object": "chat.completion",
                    "created": int(time.time()),
                    "model": "qwen",
                    "choices": [{"index": 0, "message": {"role": "assistant", "content": content}, "finish_reason": "stop"}],
                })
            except Exception as exc:
                self._json(500, {"error": str(exc)[:300]})

        def log_message(self, _format: str, *_args: object) -> None:
            return

    return Handler


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model-dir", required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()
    runtime = QwenRuntime(args.model_dir)
    ThreadingHTTPServer((args.host, args.port), make_handler(runtime)).serve_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
