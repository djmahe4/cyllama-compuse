"""
Element Engine Python wrapper.

Tries to import the compiled C++ module ``element_engine_cpp``.
If unavailable, all public functions return empty lists / None so the
caller can fall back gracefully to OCR.
"""

from __future__ import annotations

import logging
from typing import Any

logger = logging.getLogger(__name__)

# Try to import the compiled C++ extension
try:
    import element_engine_cpp as _cpp  # type: ignore[import]
    _ENGINE_AVAILABLE = True
except ImportError:
    _cpp = None
    _ENGINE_AVAILABLE = False
    logger.debug("element_engine_cpp not available; element engine disabled")


def is_available() -> bool:
    """Return True if the C++ element engine module is compiled and importable."""
    return _ENGINE_AVAILABLE


class ElementEngineSession:
    """Context-manager wrapper around the C++ Engine.

    Usage::

        with ElementEngineSession() as session:
            elements = session.enumerate()
            for el in elements:
                print(el.to_dict())
    """

    def __init__(self) -> None:
        self._engine: Any = None
        if _ENGINE_AVAILABLE:
            try:
                self._engine = _cpp.Engine()
            except Exception as exc:
                logger.warning("Failed to create Engine: %s", exc)
                self._engine = None

    # ------------------------------------------------------------------
    # Context manager protocol
    # ------------------------------------------------------------------

    def __enter__(self) -> "ElementEngineSession":
        return self

    def __exit__(self, *_: Any) -> None:
        self.close()

    def close(self) -> None:
        """Destroy the underlying engine and release OS resources."""
        if self._engine is not None:
            try:
                del self._engine
            except Exception:
                pass
            self._engine = None

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def enumerate(self, max_elements: int = 0) -> list[dict]:
        """Return a list of element dicts from the accessibility API.

        Returns an empty list if the engine is unavailable.
        """
        if self._engine is None:
            return []
        try:
            infos = self._engine.enumerate(max_elements)
            return [info.to_dict() for info in infos]
        except Exception as exc:
            logger.warning("enumerate() failed: %s", exc)
            return []

    def get_info(self, element_id: Any) -> dict | None:
        """Return info for a single element by its ElementId.  None if not found."""
        if self._engine is None:
            return None
        try:
            info = self._engine.get_info(element_id)
            return info.to_dict() if info is not None else None
        except Exception as exc:
            logger.warning("get_info() failed: %s", exc)
            return None

    def click(self, element_id: Any) -> int:
        """Click an element.  Returns 0 on success, -1 on failure."""
        if self._engine is None:
            return -1
        try:
            return self._engine.click(element_id)
        except Exception as exc:
            logger.warning("click() failed: %s", exc)
            return -1

    def type_text(self, element_id: Any, text: str) -> int:
        """Type text into an element.  Returns 0 on success."""
        if self._engine is None:
            return -1
        try:
            return self._engine.type_text(element_id, text)
        except Exception as exc:
            logger.warning("type_text() failed: %s", exc)
            return -1

    def flush_cache(self) -> None:
        """Flush the internal element identity cache."""
        if self._engine is not None:
            try:
                self._engine.flush_cache()
            except Exception as exc:
                logger.warning("flush_cache() failed: %s", exc)
