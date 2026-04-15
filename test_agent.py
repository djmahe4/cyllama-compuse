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


# ---------------------------------------------------------------------------
# TestRequestConfirmation — bidirectional safety-confirmation protocol
# ---------------------------------------------------------------------------

class TestRequestConfirmation:
    """Tests for request_confirmation and its integration in the agent loop."""

    def _make_action(self, action_type: str = "type") -> dict:
        return {"action": action_type, "text": "test input", "reason": "testing"}

    # ------------------------------------------------------------------
    # Unit tests — request_confirmation in isolation
    # ------------------------------------------------------------------

    @patch("agent._append_log")
    def test_emits_confirmation_request_event(self, mock_log, capsys):
        """request_confirmation must emit a confirmation_request JSON event to stdout."""
        action = self._make_action()
        with patch("sys.stdin") as mock_stdin:
            mock_stdin.isatty.return_value = False
            mock_stdin.readline.return_value = "CONFIRM:n\n"
            request_confirmation(action)

        captured = capsys.readouterr()
        event = json.loads(captured.out.strip())
        assert event["type"] == "confirmation_request"
        assert event["action"]["action"] == "type"
        assert "Dangerous action" in event["message"]
        assert "timestamp" in event

    @patch("agent._append_log")
    def test_confirm_y_returns_true(self, mock_log, capsys):
        """CONFIRM:y response must approve the action (return True)."""
        action = self._make_action("hotkey")
        with patch("sys.stdin") as mock_stdin:
            mock_stdin.isatty.return_value = False
            mock_stdin.readline.return_value = "CONFIRM:y\n"
            result = request_confirmation(action)
        assert result is True

    @patch("agent._append_log")
    def test_confirm_yes_returns_true(self, mock_log, capsys):
        """CONFIRM:yes (full word) must also approve."""
        action = self._make_action()
        with patch("sys.stdin") as mock_stdin:
            mock_stdin.isatty.return_value = False
            mock_stdin.readline.return_value = "CONFIRM:yes\n"
            result = request_confirmation(action)
        assert result is True

    @patch("agent._append_log")
    def test_confirm_n_returns_false(self, mock_log, capsys):
        """CONFIRM:n response must deny the action (return False)."""
        action = self._make_action()
        with patch("sys.stdin") as mock_stdin:
            mock_stdin.isatty.return_value = False
            mock_stdin.readline.return_value = "CONFIRM:n\n"
            result = request_confirmation(action)
        assert result is False

    @patch("agent._append_log")
    def test_empty_response_returns_false(self, mock_log, capsys):
        """An empty line must default to deny."""
        action = self._make_action()
        with patch("sys.stdin") as mock_stdin:
            mock_stdin.isatty.return_value = False
            mock_stdin.readline.return_value = "\n"
            result = request_confirmation(action)
        assert result is False

    @patch("agent._append_log")
    def test_garbage_prefix_returns_false(self, mock_log, capsys):
        """A line without the CONFIRM: prefix must default to deny."""
        action = self._make_action()
        with patch("sys.stdin") as mock_stdin:
            mock_stdin.isatty.return_value = False
            mock_stdin.readline.return_value = "yes\n"
            result = request_confirmation(action)
        assert result is False

    @patch("agent._append_log")
    def test_logs_approved_decision(self, mock_log, capsys):
        """Every approval decision must be persisted via _append_log."""
        action = self._make_action()
        with patch("sys.stdin") as mock_stdin:
            mock_stdin.isatty.return_value = False
            mock_stdin.readline.return_value = "CONFIRM:y\n"
            request_confirmation(action)

        mock_log.assert_called_once()
        logged = mock_log.call_args[0][0]
        assert logged["event"] == "safety_decision"
        assert logged["approved"] is True

    @patch("agent._append_log")
    def test_logs_denied_decision(self, mock_log, capsys):
        """Every denial decision must also be persisted."""
        action = self._make_action()
        with patch("sys.stdin") as mock_stdin:
            mock_stdin.isatty.return_value = False
            mock_stdin.readline.return_value = "CONFIRM:n\n"
            request_confirmation(action)

        mock_log.assert_called_once()
        logged = mock_log.call_args[0][0]
        assert logged["event"] == "safety_decision"
        assert logged["approved"] is False

    # ------------------------------------------------------------------
    # Integration tests — agent loop honours confirmation results
    # ------------------------------------------------------------------

    @patch("agent.call_ollama")
    @patch("agent._append_log")
    def test_dangerous_action_blocked_in_agent_loop(self, mock_log, mock_ollama):
        """Agent loop must NOT call execute_action when confirmation is denied."""
        mock_ollama.side_effect = [
            '{"action": "type", "text": "rm -rf /", "reason": "dangerous"}',
            '{"action": "done", "reason": "finished"}',
        ]
        mock_computer = MagicMock()
        mock_computer.capture_screen.return_value = MagicMock()
        mock_computer.run_ocr.return_value = []

        with patch("sys.stdin") as mock_stdin:
            mock_stdin.isatty.return_value = False
            mock_stdin.readline.return_value = "CONFIRM:n\n"
            with patch("agent.CONFIRM_DANGEROUS", True):
                agent = DesktopAgent(
                    task="dangerous task",
                    computer=mock_computer,
                    max_iterations=3,
                )
                events = list(agent.run_task())

        mock_computer.execute_action.assert_not_called()
        denied_msgs = [
            e for e in events
            if e["type"] == "log" and "denied" in e["data"].get("message", "").lower()
        ]
        assert len(denied_msgs) >= 1

    @patch("agent.call_ollama")
    @patch("agent._append_log")
    def test_dangerous_action_proceeds_when_confirmed(self, mock_log, mock_ollama):
        """Agent loop must call execute_action when the user confirms."""
        mock_ollama.side_effect = [
            '{"action": "type", "text": "hello world", "reason": "typing"}',
            '{"action": "done", "reason": "done"}',
        ]
        mock_computer = MagicMock()
        mock_computer.capture_screen.return_value = MagicMock()
        mock_computer.run_ocr.return_value = []
        mock_computer.execute_action.return_value = "typed"

        with patch("sys.stdin") as mock_stdin:
            mock_stdin.isatty.return_value = False
            mock_stdin.readline.return_value = "CONFIRM:y\n"
            with patch("agent.CONFIRM_DANGEROUS", True):
                agent = DesktopAgent(
                    task="type task",
                    computer=mock_computer,
                    max_iterations=3,
                )
                events = list(agent.run_task())

        mock_computer.execute_action.assert_called_once()

    @patch("agent.call_ollama")
    @patch("agent._append_log")
    def test_confirm_dangerous_false_skips_prompt(self, mock_log, mock_ollama):
        """When CONFIRM_DANGEROUS is False, dangerous actions run without prompting."""
        mock_ollama.side_effect = [
            '{"action": "type", "text": "hello", "reason": "typing"}',
            '{"action": "done", "reason": "done"}',
        ]
        mock_computer = MagicMock()
        mock_computer.capture_screen.return_value = MagicMock()
        mock_computer.run_ocr.return_value = []
        mock_computer.execute_action.return_value = "typed"

        with patch("agent.CONFIRM_DANGEROUS", False):
            agent = DesktopAgent(
                task="type without confirm",
                computer=mock_computer,
                max_iterations=3,
            )
            events = list(agent.run_task())

        # execute_action called without any stdin interaction
        mock_computer.execute_action.assert_called_once()


