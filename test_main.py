"""
Tests for the CYLLAMA COMPUSE main CLI module.
"""

import json
import sys
import pytest
from unittest.mock import patch, MagicMock
from io import StringIO

import main


class TestParseArgs:
    def test_task_argument(self):
        with patch("sys.argv", ["main.py", "open notepad"]):
            args = main.parse_args()
            assert args.task == "open notepad"

    def test_model_flag(self):
        with patch("sys.argv", ["main.py", "task", "--model", "llama3:70b"]):
            args = main.parse_args()
            assert args.model == "llama3:70b"

    def test_max_iterations(self):
        with patch("sys.argv", ["main.py", "task", "--max-iterations", "10"]):
            args = main.parse_args()
            assert args.max_iterations == 10

    def test_interactive_flag(self):
        with patch("sys.argv", ["main.py", "--interactive"]):
            args = main.parse_args()
            assert args.interactive is True

    def test_verbose_flag(self):
        with patch("sys.argv", ["main.py", "task", "--verbose"]):
            args = main.parse_args()
            assert args.verbose is True


class TestMain:
    @patch("main.DesktopAgent")
    def test_main_with_task(self, mock_agent_cls):
        mock_agent = MagicMock()
        mock_agent.run_task.return_value = iter(
            [
                {"type": "log", "data": {"message": "starting"}},
                {"type": "done", "data": {"iterations": 1, "task": "test"}},
            ]
        )
        mock_agent_cls.return_value = mock_agent

        captured = StringIO()
        with (
            patch("sys.argv", ["main.py", "test task"]),
            patch("sys.stdout", captured),
        ):
            main.main()

        mock_agent_cls.assert_called_once()
        assert mock_agent_cls.call_args.kwargs["task"] == "test task"
        mock_agent.run_task.assert_called_once()

    @patch("main.DesktopAgent")
    def test_main_no_task_exits(self, mock_agent_cls):
        """When no task is provided and stdin is empty, main exits with code 1."""
        with (
            patch("sys.argv", ["main.py"]),
            patch("sys.stdin", StringIO("")),
            patch("sys.stdout", StringIO()),
            pytest.raises(SystemExit) as exc_info,
        ):
            main.main()

        assert exc_info.value.code == 1


if __name__ == "__main__":
    pytest.main([__file__])

