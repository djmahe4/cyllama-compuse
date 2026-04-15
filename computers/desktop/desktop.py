"""
CYLLAMA COMPUSE — Desktop Computer

Provides universal desktop control via PyAutoGUI and OCR via EasyOCR.
This module can drive ANY desktop application, not just browsers.
"""

import io
import logging
import time
from typing import Optional

from PIL import Image

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Lazy-loaded heavy dependencies
# ---------------------------------------------------------------------------

_pyautogui = None
_easyocr_reader = None


def _get_pyautogui():
    """Lazy-import pyautogui so tests can run without a display."""
    global _pyautogui
    if _pyautogui is None:
        import pyautogui

        pyautogui.FAILSAFE = True
        pyautogui.PAUSE = 0.3
        _pyautogui = pyautogui
    return _pyautogui


def _get_ocr_reader():
    """Lazy-initialize the EasyOCR reader (downloads model on first run)."""
    global _easyocr_reader
    if _easyocr_reader is None:
        try:
            import easyocr

            _easyocr_reader = easyocr.Reader(["en"], gpu=False)
        except ImportError:
            logger.warning("easyocr not installed – OCR disabled")
            _easyocr_reader = None
    return _easyocr_reader


# ---------------------------------------------------------------------------
# DesktopComputer
# ---------------------------------------------------------------------------


class DesktopComputer:
    """Universal desktop control surface.

    Exposes:
        capture_screen()           → PIL.Image
        run_ocr(image)             → list[dict]   (structured elements)
        execute_action(action_dict) → str          (result description)
    """

    def __init__(self):
        self._ocr_reader = None  # lazy

    # ------------------------------------------------------------------
    # Screen capture
    # ------------------------------------------------------------------

    def capture_screen(self) -> Image.Image:
        """Take a screenshot of the entire primary monitor.

        Returns a PIL Image in RGB mode.
        """
        pag = _get_pyautogui()
        screenshot = pag.screenshot()
        return screenshot

    # ------------------------------------------------------------------
    # OCR
    # ------------------------------------------------------------------

    def run_ocr(self, image: Image.Image) -> list[dict]:
        """Run OCR on a PIL Image and return structured elements.

        Returns a list of dicts:
            [{"text": str, "x": int, "y": int, "w": int, "h": int, "confidence": float}, ...]

        The (x, y) coordinates refer to the *center* of the bounding box.
        """
        reader = _get_ocr_reader()
        if reader is None:
            return []

        # EasyOCR can process PIL images directly, avoiding redundant encoding/decoding
        results = reader.readtext(image)
        elements: list[dict] = []
        for bbox, text, confidence in results:
            # bbox is [[x1,y1],[x2,y1],[x2,y2],[x1,y2]]
            xs = [pt[0] for pt in bbox]
            ys = [pt[1] for pt in bbox]
            x_min, x_max = int(min(xs)), int(max(xs))
            y_min, y_max = int(min(ys)), int(max(ys))
            w = x_max - x_min
            h = y_max - y_min
            cx = x_min + w // 2
            cy = y_min + h // 2
            elements.append(
                {
                    "text": text,
                    "x": cx,
                    "y": cy,
                    "w": w,
                    "h": h,
                    "confidence": round(float(confidence), 4),
                }
            )
        return elements

    # ------------------------------------------------------------------
    # Action execution
    # ------------------------------------------------------------------

    def execute_action(self, action: dict) -> str:
        """Execute a desktop action described by *action*.

        Supported actions:
            click   – move mouse and click at (x, y)
            type    – type text string
            hotkey  – press key combination
            scroll  – scroll up or down
        """
        pag = _get_pyautogui()
        action_type = action.get("action", "")

        if action_type == "click":
            x = int(action.get("x", 0))
            y = int(action.get("y", 0))
            button = action.get("button", "left")
            pag.click(x, y, button=button)
            return f"clicked ({x}, {y})"

        elif action_type == "type":
            text = action.get("text", "")
            interval = action.get("interval", 0.03)
            pag.write(text, interval=interval)
            return f"typed {len(text)} chars"

        elif action_type == "hotkey":
            keys = action.get("keys", [])
            if isinstance(keys, str):
                keys = [keys]
            pag.hotkey(*keys)
            return f"hotkey {'+'.join(keys)}"

        elif action_type == "scroll":
            direction = action.get("direction", "down")
            amount = int(action.get("amount", 3))
            clicks = amount if direction == "up" else -amount
            x = action.get("x")
            y = action.get("y")
            if x is not None and y is not None:
                pag.scroll(clicks, x=int(x), y=int(y))
            else:
                pag.scroll(clicks)
            return f"scrolled {direction} {amount}"

        elif action_type == "done":
            return "done"

        else:
            raise ValueError(f"Unknown action type: {action_type}")
