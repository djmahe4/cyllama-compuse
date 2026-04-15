"""
CYLLAMA COMPUSE — Agent Loop

Ollama-powered desktop automation agent.
Implements the perception → reasoning → action loop.
"""

import os
import sys
import json
import time
import logging
from typing import Generator, Optional

import requests

from computers.desktop.desktop import DesktopComputer

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
OLLAMA_HOST = os.environ.get("OLLAMA_HOST", "http://localhost:11434")
MODEL_NAME = os.environ.get("MODEL_NAME", "llama3:8b")
MAX_ITERATIONS = 30
LOG_DIR = os.environ.get("LOG_DIR", "logs")
LOG_FILE = os.environ.get("LOG_FILE", "agent-history.json")

# Actions considered dangerous and requiring confirmation
DANGEROUS_ACTIONS = {"hotkey", "type"}

SYSTEM_PROMPT = """\
You are CYLLAMA COMPUSE, an AI desktop automation agent.
You can see the user's screen via OCR and execute actions on their desktop.

You MUST respond with a single valid JSON object. Do NOT include any other text.
The JSON MUST contain these keys:
- "action": one of "click", "type", "hotkey", "scroll", "done"
- "reason": a short explanation of why you chose this action

Additional keys depending on the action:
- click:  "x" (int), "y" (int)
- type:   "text" (str)
- hotkey: "keys" (list of str, e.g. ["ctrl", "c"])
- scroll: "direction" ("up" or "down"), "amount" (int, default 3)
- done:   (no extra keys)

Example responses:
{"action": "click", "x": 500, "y": 300, "reason": "clicking the search bar"}
{"action": "type", "text": "hello world", "reason": "typing into the search bar"}
{"action": "hotkey", "keys": ["ctrl", "s"], "reason": "saving the document"}
{"action": "scroll", "direction": "down", "amount": 5, "reason": "scrolling to see more"}
{"action": "done", "reason": "task is complete"}
"""


def _emit(event_type: str, data: dict) -> dict:
    """Build a JSON event to send to the TUI."""
    return {"type": event_type, "data": data}


def _ensure_log_dir() -> str:
    """Create the log directory if needed and return the log file path."""
    os.makedirs(LOG_DIR, exist_ok=True)
    return os.path.join(LOG_DIR, LOG_FILE)


def _append_log(entry: dict) -> None:
    """Append an entry to the JSON log file."""
    log_path = _ensure_log_dir()
    history: list = []
    if os.path.exists(log_path):
        try:
            with open(log_path, "r") as f:
                history = json.load(f)
        except (json.JSONDecodeError, IOError):
            history = []
    history.append(entry)
    with open(log_path, "w") as f:
        json.dump(history, f, indent=2)


# ---------------------------------------------------------------------------
# Ollama client
# ---------------------------------------------------------------------------

def call_ollama(messages: list[dict], model: str = MODEL_NAME) -> str:
    """Call the Ollama chat API and return the assistant's response text."""
    url = f"{OLLAMA_HOST}/api/chat"
    payload = {
        "model": model,
        "messages": messages,
        "stream": False,
        "format": "json",
        "options": {
            "temperature": 0.2,
            "num_predict": 512,
        },
    }
    try:
        resp = requests.post(url, json=payload, timeout=120)
        resp.raise_for_status()
        body = resp.json()
        return body.get("message", {}).get("content", "")
    except requests.RequestException as exc:
        logger.error("Ollama request failed: %s", exc)
        raise


def parse_action(raw: str) -> dict:
    """Parse the model response into an action dict.

    Tries to extract a JSON object from the raw text.  Falls back to a
    ``done`` action if parsing fails.
    """
    raw = raw.strip()
    # Try to find a JSON object in the response
    start = raw.find("{")
    end = raw.rfind("}")
    if start != -1 and end != -1 and end > start:
        try:
            return json.loads(raw[start : end + 1])
        except json.JSONDecodeError:
            pass
    return {"action": "done", "reason": "could not parse model response"}


# ---------------------------------------------------------------------------
# Safety
# ---------------------------------------------------------------------------

def _is_dangerous(action: dict) -> bool:
    """Return True if the action is considered potentially dangerous."""
    return action.get("action") in DANGEROUS_ACTIONS


def confirm_action(action: dict) -> bool:
    """Prompt the user to confirm a dangerous action.

    Returns True if the user confirms, False otherwise.  When
    ``CONFIRM_DANGEROUS`` env var is ``false`` confirmation is skipped.
    """
    if os.environ.get("CONFIRM_DANGEROUS", "true").lower() == "false":
        return True
    print(
        json.dumps(
            _emit("log", {"message": f"⚠️  Confirm action: {json.dumps(action)}"})
        ),
        flush=True,
    )
    # In non-interactive (piped) mode, auto-confirm
    if not sys.stdin.isatty():
        return True
    answer = input("Proceed? [y/N] ").strip().lower()
    return answer in ("y", "yes")


# ---------------------------------------------------------------------------
# Agent
# ---------------------------------------------------------------------------

class DesktopAgent:
    """Ollama-powered desktop automation agent."""

    def __init__(
        self,
        task: str,
        *,
        model: str = MODEL_NAME,
        max_iterations: int = MAX_ITERATIONS,
        computer: Optional[DesktopComputer] = None,
    ):
        self.task = task
        self.model = model
        self.max_iterations = max_iterations
        self.computer = computer or DesktopComputer()
        self.history: list[dict] = []
        self._messages: list[dict] = [{"role": "system", "content": SYSTEM_PROMPT}]

    # ------------------------------------------------------------------
    # Main loop exposed as a generator of JSON events
    # ------------------------------------------------------------------

    def run_task(self, task: Optional[str] = None) -> Generator[dict, None, None]:
        """Run the agent loop, yielding JSON events for each step."""
        task = task or self.task
        yield _emit("log", {"message": f"Starting task: {task}"})

        # Initial user message with task description
        self._messages.append(
            {
                "role": "user",
                "content": f"TASK: {task}\n\nPlease analyze the current screen and decide the first action.",
            }
        )

        for iteration in range(1, self.max_iterations + 1):
            yield _emit("log", {"message": f"--- Iteration {iteration} ---"})

            # 1. Capture screenshot
            try:
                screenshot = self.computer.capture_screen()
            except Exception as exc:
                yield _emit("log", {"message": f"Screenshot failed: {exc}"})
                break

            # 2. Perception — run OCR
            try:
                ocr_elements = self.computer.run_ocr(screenshot)
            except Exception as exc:
                yield _emit("log", {"message": f"OCR failed: {exc}"})
                ocr_elements = []

            yield _emit("ocr", {"elements": ocr_elements})

            # Sort elements by position (top-to-bottom, left-to-right) before truncation
            sorted_elements = sorted(ocr_elements, key=lambda e: (e["y"], e["x"]))
            
            ocr_summary = json.dumps(sorted_elements[:50])
            # 3. Build prompt with OCR context
            ocr_summary = json.dumps(ocr_elements[:50])  # limit to 50 elements
            self._messages.append(
                {
                    "role": "user",
                    "content": (
                        f"Current screen OCR elements (up to 50):\n{ocr_summary}\n\n"
                        "Based on the current screen state, what action should I take next? "
                        "Respond with a JSON object."
                    ),
                }
            )

            # 4. Call Ollama
            yield _emit("log", {"message": "Calling Ollama..."})
            try:
                raw_response = call_ollama(self._messages, self.model)
            except Exception as exc:
                yield _emit("log", {"message": f"Ollama error: {exc}"})
                break

            action = parse_action(raw_response)
            yield _emit("action", action)

            # Append model response to message history
            self._messages.append(
                {"role": "assistant", "content": json.dumps(action)}
            )

            # 5. Check for done
            if action.get("action") == "done":
                yield _emit("log", {"message": f"Task complete: {action.get('reason', '')}"})
                _append_log(
                    {
                        "task": task,
                        "iterations": iteration,
                        "result": "done",
                        "reason": action.get("reason", ""),
                        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                    }
                )
                break

            # 6. Safety check
            if _is_dangerous(action) and not confirm_action(action):
                yield _emit("log", {"message": "Action cancelled by user."})
                _append_log(
                    {
                        "task": task,
                        "iterations": iteration,
                        "result": "cancelled",
                        "action": action,
                        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                    }
                )
                break

            # 7. Execute action
            try:
                result = self.computer.execute_action(action)
                yield _emit(
                    "log", {"message": f"Action executed: {action.get('action')} → {result}"}
                )
            except Exception as exc:
                yield _emit("log", {"message": f"Execution error: {exc}"})
                self._messages.append(
                    {
                        "role": "user",
                        "content": f"The action failed with error: {exc}. Please try a different approach.",
                    }
                )

            # 8. Update history
            self.history.append(
                {
                    "iteration": iteration,
                    "ocr_count": len(ocr_elements),
                    "action": action,
                    "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                }
            )

            # Brief pause between iterations
            time.sleep(0.5)
        else:
            yield _emit(
                "log",
                {"message": f"Max iterations ({self.max_iterations}) reached."},
            )

        yield _emit("done", {"iterations": len(self.history), "task": task})

