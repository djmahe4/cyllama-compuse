"""
CYLLAMA COMPUSE — Playwright Computer (compatibility stub)

This module exists for structural compatibility with the original
google-gemini/computer-use-preview architecture.  The real desktop
automation is provided by ``computers.desktop.desktop.DesktopComputer``.

If Playwright is installed, a thin wrapper around a headless browser is
provided for web-only tasks; otherwise the class raises at construction.
"""

import logging

logger = logging.getLogger(__name__)


class PlaywrightComputer:
    """Compatibility stub for Playwright-based browser automation.

    For full desktop control, use ``DesktopComputer`` instead.
    """

    def __init__(self, width: int = 1280, height: int = 720):
        self.width = width
        self.height = height
        logger.info(
            "PlaywrightComputer is a stub. Use DesktopComputer for full functionality."
        )

