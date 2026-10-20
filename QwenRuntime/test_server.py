"""Local runtime contract checks; no model weights or third-party packages needed."""

import json
import threading
import unittest
from http.client import HTTPConnection
from http.server import ThreadingHTTPServer

from qwen_local_server import QwenRuntime, make_handler, normalize_local_identity, safe_local_answer


class FixtureRuntime(QwenRuntime):
    """Replace only expensive model inference, keeping real routing and HTTP code."""

    def __init__(self, answer):
        self.answer = answer
        self.generation_lock = threading.Lock()

    def _generate(self, messages, max_tokens):
        return self.answer


class IdentityTests(unittest.TestCase):
    def test_cloud_identity_describes_this_local_ue_project(self):
        for answer in ("我是阿里云助手", "使用云端服务", "A CLOUD SERVICE"):
            with self.subTest(answer=answer):
                result = normalize_local_identity(answer)
                self.assertIn("本机部署", result)
                self.assertIn("UE场景助手", result)
                self.assertNotIn("CATIA", result)

    def test_safe_fallback_asks_for_a_scene_without_suggesting_execution(self):
        for answer in ("", "```python\nexec('x')\n```", "powershell Remove-Item x"):
            with self.subTest(answer=answer):
                result = safe_local_answer(answer)
                self.assertIn("场景", result)
                self.assertNotIn("零件", result)
                self.assertNotIn("exec(", result)
                self.assertNotIn("powershell", result)

    def test_ordinary_text_is_preserved_and_long_fallbacks_are_bounded(self):
        self.assertEqual(normalize_local_identity("本地飞机编队演示"), "本地飞机编队演示")
        self.assertEqual(safe_local_answer("  查看飞机编号  "), "查看飞机编号")
        self.assertEqual(len(safe_local_answer("甲" * 1000)), 800)


class RoutingTests(unittest.TestCase):
    def test_current_ue_text_and_command_json_pass_through_unchanged(self):
        messages = [{"role": "system", "content": "你是本地UE场景助手，用中文简短回答。"}]
        for answer in ("请点击应用到场景。", '{"update":"aircraft","set":{"height":30}}'):
            with self.subTest(answer=answer):
                self.assertEqual(FixtureRuntime(answer).chat(messages), answer)

    def test_legacy_message_normalizes_local_identity(self):
        messages = [{"role": "system", "content": "Return exactly one JSON object."}]
        result = json.loads(FixtureRuntime('{"kind":"message","text":"阿里云助手"}').chat(messages))
        self.assertEqual(result["kind"], "message")
        self.assertIn("UE场景助手", result["text"])
        self.assertNotIn("CATIA", result["text"])

    def test_legacy_proposal_and_fallback_routing_are_preserved(self):
        messages = [{"role": "system", "content": "Return exactly one JSON object."}]
        proposal = {"kind": "part_proposal", "parameters": {"width": 10}, "suggestions": []}
        self.assertEqual(json.loads(FixtureRuntime(json.dumps(proposal)).chat(messages)), proposal)
        fallback = json.loads(FixtureRuntime("```python\nexec('x')\n```").chat(messages))
        self.assertEqual(fallback["kind"], "message")
        self.assertEqual(fallback["parameters"], {})
        self.assertEqual(fallback["suggestions"], [])
        self.assertIn("场景", fallback["text"])


class HttpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.runtime = FixtureRuntime('{"update":"aircraft","set":{"height":30}}')
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), make_handler(cls.runtime))
        cls.worker = threading.Thread(target=lambda: cls.server.serve_forever(poll_interval=0.05), daemon=True)
        cls.worker.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.worker.join(timeout=2)

    def request_json(self, method, path, payload=None):
        connection = HTTPConnection(*self.server.server_address, timeout=2)
        try:
            body = None if payload is None else json.dumps(payload)
            connection.request(method, path, body=body, headers={"Content-Type": "application/json"})
            response = connection.getresponse()
            return response.status, json.loads(response.read())
        finally:
            connection.close()

    def test_health_and_model_discovery_remain_compatible(self):
        status, health = self.request_json("GET", "/health")
        self.assertEqual((status, health), (200, {"status": "ok", "model": "qwen"}))
        status, models = self.request_json("GET", "/v1/models")
        self.assertEqual(status, 200)
        self.assertEqual(models["data"], [{"id": "qwen", "object": "model"}])

    def test_completion_identifies_tactical_runtime_and_preserves_scene_json(self):
        status, response = self.request_json("POST", "/v1/chat/completions", {
            "model": "qwen", "messages": [{"role": "system", "content": "提取场景修改指令，只输出JSON。"}]
        })
        self.assertEqual(status, 200)
        self.assertEqual(response["id"], "tactical-demo-qwen-local")
        self.assertEqual(response["object"], "chat.completion")
        self.assertEqual(response["model"], "qwen")
        self.assertEqual(response["choices"][0]["message"], {
            "role": "assistant", "content": '{"update":"aircraft","set":{"height":30}}'
        })
        self.assertEqual(response["choices"][0]["finish_reason"], "stop")

    def test_invalid_model_is_rejected_without_breaking_health(self):
        status, response = self.request_json("POST", "/v1/chat/completions", {"model": "other", "messages": []})
        self.assertEqual(status, 500)
        self.assertEqual(response["error"], "model must be qwen")
        self.assertEqual(self.request_json("GET", "/health")[0], 200)


if __name__ == "__main__":
    unittest.main()
