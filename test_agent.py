"""
Tests for the CYLLAMA COMPUSE agent module.

These tests mock out external dependencies (Ollama, PyAutoGUI, EasyOCR)
so they can run in CI without a display server.
"""

import json
import pytest
from unittest.mock import patch, MagicMock
from io import StringIO

from agent import (
    DesktopAgent,
    call_ollama,
    parse_action,
    _is_dangerous,
    _emit,
    request_confirmation,
)


# ---------------------------------------------------------------------------
# Unit tests for parse_action
# ---------------------------------------------------------------------------

class TestParseAction:
    def test_valid_click(self):
        raw = '{"action": "click", "x": 100, "y": 200, "reason": "test"}'
        result = parse_action(raw)
        assert result["action"] == "click"
        assert result["x"] == 100
        assert result["y"] == 200

    def test_valid_type(self):
        raw = '{"action": "type", "text": "hello", "reason": "typing"}'
        result = parse_action(raw)
        assert result["action"] == "type"
        assert result["text"] == "hello"

    def test_valid_done(self):
        raw = '{"action": "done", "reason": "task complete"}'
        result = parse_action(raw)
        assert result["action"] == "done"

    def test_malformed_json(self):
        raw = "this is not json"
        result = parse_action(raw)
        assert result["action"] == "done"
        assert "could not parse" in result["reason"]

    def test_json_embedded_in_text(self):
        raw = 'Here is my response: {"action": "scroll", "direction": "down", "amount": 3, "reason": "reading"} end'
        result = parse_action(raw)
        assert result["action"] == "scroll"
        assert result["direction"] == "down"

    def test_empty_string(self):
        result = parse_action("")
        assert result["action"] == "done"


# ---------------------------------------------------------------------------
# Unit tests for _is_dangerous
# ---------------------------------------------------------------------------

class TestIsDangerous:
    def test_click_not_dangerous(self):
        assert _is_dangerous({"action": "click", "x": 10, "y": 20}) is False

    def test_type_is_dangerous(self):
        assert _is_dangerous({"action": "type", "text": "hello"}) is True

    def test_hotkey_is_dangerous(self):
        assert _is_dangerous({"action": "hotkey", "keys": ["ctrl", "c"]}) is True

    def test_scroll_not_dangerous(self):
        assert _is_dangerous({"action": "scroll", "direction": "up"}) is False

    def test_done_not_dangerous(self):
        assert _is_dangerous({"action": "done"}) is False


# ---------------------------------------------------------------------------
# Unit tests for _emit
# ---------------------------------------------------------------------------

class TestEmit:
    def test_log_event(self):
        event = _emit("log", {"message": "hello"})
        assert event == {"type": "log", "data": {"message": "hello"}}

    def test_done_event(self):
        event = _emit("done", {"iterations": 5, "task": "test"})
        assert event["type"] == "done"
        assert event["data"]["iterations"] == 5


# ---------------------------------------------------------------------------
# call_ollama tests (mocked)
# ---------------------------------------------------------------------------

class TestCallOllama:
    @patch("agent.requests.post")
    def test_successful_call(self, mock_post):
        mock_resp = MagicMock()
        mock_resp.status_code = 200
        mock_resp.json.return_value = {
            "message": {
                "content": '{"action": "click", "x": 50, "y": 50, "reason": "test"}'
            }
        }
        mock_resp.raise_for_status = MagicMock()
        mock_post.return_value = mock_resp

        result = call_ollama([{"role": "user", "content": "test"}])
        assert "click" in result

    @patch("agent.requests.post")
    def test_failed_call(self, mock_post):
        import requests as req

        mock_post.side_effect = req.RequestException("Connection refused")
        with pytest.raises(req.RequestException):
            call_ollama([{"role": "user", "content": "test"}])


# ---------------------------------------------------------------------------
# DesktopAgent tests (mocked computer + ollama)
# ---------------------------------------------------------------------------

class TestDesktopAgent:
    def _make_agent(self, mock_computer=None):
        if mock_computer is None:
            mock_computer = MagicMock()
            mock_computer.capture_screen.return_value = MagicMock()
            mock_computer.run_ocr.return_value = [
                {"text": "OK", "x": 100, "y": 200, "w": 40, "h": 20, "confidence": 0.95}
            ]
            mock_computer.execute_action.return_value = "clicked (100, 200)"
        return DesktopAgent(task="test task", computer=mock_computer, max_iterations=3)

    @patch("agent.call_ollama")
    def test_run_task_immediate_done(self, mock_ollama):
        mock_ollama.return_value = '{"action": "done", "reason": "already done"}'
        agent = self._make_agent()
        events = list(agent.run_task())

        types = [e["type"] for e in events]
        assert "done" in types

    @patch("agent.call_ollama")
    def test_run_task_click_then_done(self, mock_ollama):
        mock_ollama.side_effect = [
            '{"action": "click", "x": 100, "y": 200, "reason": "click button"}',
            '{"action": "done", "reason": "all done"}',
        ]
        agent = self._make_agent()
        events = list(agent.run_task())

        action_events = [e for e in events if e["type"] == "action"]
        assert len(action_events) >= 1
        assert action_events[0]["data"]["action"] == "click"

    @patch("agent.call_ollama")
    def test_run_task_max_iterations(self, mock_ollama):
        mock_ollama.return_value = '{"action": "click", "x": 50, "y": 50, "reason": "keep going"}'
        agent = self._make_agent()
        events = list(agent.run_task())

        # Should have 3 iterations (max_iterations=3) plus done event
        done_events = [e for e in events if e["type"] == "done"]
        assert len(done_events) == 1

    @patch("agent.call_ollama")
    def test_run_task_ocr_failure_continues(self, mock_ollama):
        mock_ollama.return_value = '{"action": "done", "reason": "done"}'
        mock_computer = MagicMock()
        mock_computer.capture_screen.return_value = MagicMock()
        mock_computer.run_ocr.side_effect = RuntimeError("OCR failed")
        mock_computer.execute_action.return_value = "ok"

        agent = self._make_agent(mock_computer)
        events = list(agent.run_task())

        # Agent should still emit done even when OCR fails
        types = [e["type"] for e in events]
        assert "done" in types

    @patch("agent.call_ollama")
    def test_history_populated(self, mock_ollama):
        mock_ollama.side_effect = [
            '{"action": "click", "x": 10, "y": 20, "reason": "step 1"}',
            '{"action": "done", "reason": "finished"}',
        ]
        agent = self._make_agent()
        list(agent.run_task())

        assert len(agent.history) == 1
        assert agent.history[0]["action"]["action"] == "click"

