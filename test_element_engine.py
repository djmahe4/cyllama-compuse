"""
Tests for the element engine wrapper and integration.

These tests run without requiring the compiled C++ module — they exercise the
Python wrapper's graceful degradation when the native extension is absent.
"""

from __future__ import annotations

import pytest
from unittest.mock import MagicMock, patch


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_mock_element_info(name: str = "OK", role: str = "button",
                            x: int = 100, y: int = 200) -> dict:
    """Return a fake element dict matching the ElementInfo.to_dict() schema."""
    return {
        "id": {
            "process_id": 1234,
            "window_handle": 5678,
            "role": 2,
            "checksum": 0xDEADBEEF,
            "automation_id_hash": 0,
            "source": 0,
        },
        "name": name,
        "role_name": role,
        "value": "",
        "x": x,
        "y": y,
        "width": 80,
        "height": 30,
        "enabled": True,
        "focused": False,
        "source": "accessibility",
    }


# ---------------------------------------------------------------------------
# Tests for is_available()
# ---------------------------------------------------------------------------

class TestIsAvailable:
    def test_returns_bool(self):
        from computers.desktop.element_engine_wrapper import is_available
        result = is_available()
        assert isinstance(result, bool)

    def test_false_when_no_cpp_module(self):
        """Without the compiled extension, is_available() must return False."""
        import sys
        # Reload the wrapper with element_engine_cpp absent from sys.modules
        with patch.dict("sys.modules", {"element_engine_cpp": None}):
            import importlib
            import computers.desktop.element_engine_wrapper as wrapper
            importlib.reload(wrapper)
            assert wrapper.is_available() is False


# ---------------------------------------------------------------------------
# Tests for ElementEngineSession — engine unavailable path
# ---------------------------------------------------------------------------

class TestElementEngineSessionUnavailable:
    """Behaviour when element_engine_cpp is NOT importable."""

    def setup_method(self):
        import sys
        import importlib
        with patch.dict("sys.modules", {"element_engine_cpp": None}):
            import computers.desktop.element_engine_wrapper as wrapper
            importlib.reload(wrapper)
            self.wrapper = wrapper

    def test_enumerate_returns_empty_list(self):
        session = self.wrapper.ElementEngineSession()
        result = session.enumerate()
        assert result == []

    def test_enumerate_with_max_elements_returns_empty_list(self):
        session = self.wrapper.ElementEngineSession()
        assert session.enumerate(max_elements=10) == []

    def test_get_info_returns_none(self):
        session = self.wrapper.ElementEngineSession()
        assert session.get_info(object()) is None

    def test_click_returns_minus_one(self):
        session = self.wrapper.ElementEngineSession()
        assert session.click(object()) == -1

    def test_type_text_returns_minus_one(self):
        session = self.wrapper.ElementEngineSession()
        assert session.type_text(object(), "hello") == -1

    def test_flush_cache_does_not_raise(self):
        session = self.wrapper.ElementEngineSession()
        session.flush_cache()  # must not raise

    def test_close_does_not_raise(self):
        session = self.wrapper.ElementEngineSession()
        session.close()  # must not raise

    def test_context_manager_works(self):
        with self.wrapper.ElementEngineSession() as session:
            assert session.enumerate() == []

    def test_context_manager_exit_does_not_raise(self):
        with self.wrapper.ElementEngineSession():
            pass


# ---------------------------------------------------------------------------
# Tests for ElementEngineSession — engine available path (mocked C++ module)
# ---------------------------------------------------------------------------

class TestElementEngineSessionAvailable:
    """Behaviour when a mock element_engine_cpp IS importable."""

    def _make_mock_cpp(self, elements: list[dict] | None = None):
        """Build a mock cpp module whose Engine returns *elements*."""
        mock_info = MagicMock()
        mock_info.to_dict.return_value = elements[0] if elements else {}

        mock_engine = MagicMock()
        mock_engine.enumerate.return_value = [mock_info] if elements else []
        mock_engine.get_info.return_value = mock_info if elements else None
        mock_engine.click.return_value = 0
        mock_engine.type_text.return_value = 0
        mock_engine.flush_cache.return_value = None

        mock_cpp = MagicMock()
        mock_cpp.Engine.return_value = mock_engine
        return mock_cpp, mock_engine, mock_info

    def test_enumerate_returns_list_of_dicts(self):
        fake_el = _make_mock_element_info()
        mock_cpp, mock_engine, mock_info = self._make_mock_cpp([fake_el])

        import computers.desktop.element_engine_wrapper as wrapper
        with patch.object(wrapper, "_cpp", mock_cpp), \
             patch.object(wrapper, "_ENGINE_AVAILABLE", True):
            session = wrapper.ElementEngineSession()
            result = session.enumerate()

        assert isinstance(result, list)
        assert len(result) == 1

    def test_enumerate_empty_when_engine_returns_empty(self):
        mock_cpp, mock_engine, _ = self._make_mock_cpp([])
        import computers.desktop.element_engine_wrapper as wrapper
        with patch.object(wrapper, "_cpp", mock_cpp), \
             patch.object(wrapper, "_ENGINE_AVAILABLE", True):
            session = wrapper.ElementEngineSession()
            result = session.enumerate()
        assert result == []

    def test_get_info_returns_dict(self):
        fake_el = _make_mock_element_info()
        mock_cpp, mock_engine, mock_info = self._make_mock_cpp([fake_el])
        import computers.desktop.element_engine_wrapper as wrapper
        with patch.object(wrapper, "_cpp", mock_cpp), \
             patch.object(wrapper, "_ENGINE_AVAILABLE", True):
            session = wrapper.ElementEngineSession()
            result = session.get_info(MagicMock())
        assert result is not None

    def test_click_returns_zero_on_success(self):
        mock_cpp, mock_engine, _ = self._make_mock_cpp()
        mock_engine.click.return_value = 0
        import computers.desktop.element_engine_wrapper as wrapper
        with patch.object(wrapper, "_cpp", mock_cpp), \
             patch.object(wrapper, "_ENGINE_AVAILABLE", True):
            session = wrapper.ElementEngineSession()
            assert session.click(MagicMock()) == 0

    def test_type_text_returns_zero_on_success(self):
        mock_cpp, mock_engine, _ = self._make_mock_cpp()
        mock_engine.type_text.return_value = 0
        import computers.desktop.element_engine_wrapper as wrapper
        with patch.object(wrapper, "_cpp", mock_cpp), \
             patch.object(wrapper, "_ENGINE_AVAILABLE", True):
            session = wrapper.ElementEngineSession()
            assert session.type_text(MagicMock(), "hello") == 0

    def test_flush_cache_calls_engine(self):
        mock_cpp, mock_engine, _ = self._make_mock_cpp()
        import computers.desktop.element_engine_wrapper as wrapper
        with patch.object(wrapper, "_cpp", mock_cpp), \
             patch.object(wrapper, "_ENGINE_AVAILABLE", True):
            session = wrapper.ElementEngineSession()
            session.flush_cache()
        mock_engine.flush_cache.assert_called_once()

    def test_context_manager_calls_close(self):
        mock_cpp, mock_engine, _ = self._make_mock_cpp()
        import computers.desktop.element_engine_wrapper as wrapper
        with patch.object(wrapper, "_cpp", mock_cpp), \
             patch.object(wrapper, "_ENGINE_AVAILABLE", True):
            with wrapper.ElementEngineSession() as session:
                assert session._engine is not None
            assert session._engine is None

    def test_engine_creation_failure_is_handled(self):
        mock_cpp = MagicMock()
        mock_cpp.Engine.side_effect = RuntimeError("engine init failed")
        import computers.desktop.element_engine_wrapper as wrapper
        with patch.object(wrapper, "_cpp", mock_cpp), \
             patch.object(wrapper, "_ENGINE_AVAILABLE", True):
            session = wrapper.ElementEngineSession()
            assert session._engine is None
            assert session.enumerate() == []


# ---------------------------------------------------------------------------
# Tests for DesktopComputer.enumerate_elements()
# ---------------------------------------------------------------------------

class TestDesktopEnumerateElements:
    """Test the enumerate_elements() integration on DesktopComputer."""

    def test_returns_list(self):
        from computers.desktop.desktop import DesktopComputer
        from computers.desktop.element_engine_wrapper import ElementEngineSession
        computer = DesktopComputer()

        with patch("computers.desktop.element_engine_wrapper.is_available",
                   return_value=False), \
             patch.object(computer, "capture_screen", return_value=MagicMock()), \
             patch.object(computer, "run_ocr", return_value=[]):
            result = computer.enumerate_elements()

        assert isinstance(result, list)

    def test_uses_ocr_fallback_when_engine_unavailable(self):
        from computers.desktop.desktop import DesktopComputer
        computer = DesktopComputer()
        fake_ocr = [{"text": "Submit", "x": 100, "y": 200, "w": 60, "h": 25,
                     "confidence": 0.98}]

        with patch("computers.desktop.element_engine_wrapper.is_available",
                   return_value=False), \
             patch.object(computer, "capture_screen", return_value=MagicMock()), \
             patch.object(computer, "run_ocr", return_value=fake_ocr):
            result = computer.enumerate_elements()

        assert result == fake_ocr

    def test_uses_engine_when_available(self):
        from computers.desktop.desktop import DesktopComputer
        from computers.desktop import element_engine_wrapper as wrapper
        computer = DesktopComputer()
        fake_elements = [_make_mock_element_info("Save", "button", 50, 60)]

        mock_session = MagicMock()
        mock_session.__enter__ = MagicMock(return_value=mock_session)
        mock_session.__exit__ = MagicMock(return_value=False)
        mock_session.enumerate.return_value = fake_elements

        with patch("computers.desktop.element_engine_wrapper.is_available",
                   return_value=True), \
             patch("computers.desktop.element_engine_wrapper.ElementEngineSession",
                   return_value=mock_session):
            result = computer.enumerate_elements()

        assert result == fake_elements

    def test_falls_back_to_ocr_when_engine_returns_empty(self):
        from computers.desktop.desktop import DesktopComputer
        computer = DesktopComputer()
        fake_ocr = [{"text": "Cancel", "x": 200, "y": 300,
                     "w": 60, "h": 25, "confidence": 0.9}]

        mock_session = MagicMock()
        mock_session.__enter__ = MagicMock(return_value=mock_session)
        mock_session.__exit__ = MagicMock(return_value=False)
        mock_session.enumerate.return_value = []  # engine returns nothing

        with patch("computers.desktop.element_engine_wrapper.is_available",
                   return_value=True), \
             patch("computers.desktop.element_engine_wrapper.ElementEngineSession",
                   return_value=mock_session), \
             patch.object(computer, "capture_screen", return_value=MagicMock()), \
             patch.object(computer, "run_ocr", return_value=fake_ocr):
            result = computer.enumerate_elements()

        assert result == fake_ocr

    def test_ocr_exception_returns_empty(self):
        from computers.desktop.desktop import DesktopComputer
        computer = DesktopComputer()

        with patch("computers.desktop.element_engine_wrapper.is_available",
                   return_value=False), \
             patch.object(computer, "capture_screen",
                          side_effect=RuntimeError("no display")):
            result = computer.enumerate_elements()

        assert result == []
