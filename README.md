# CYLLAMA COMPUSE

> **Cy**ber O**llama** **Comp**uter **Use** — A fully local, offline-capable desktop automation agent.

```
 ██████╗██╗   ██╗██╗     ██╗      █████╗ ███╗   ███╗ █████╗
██╔════╝╚██╗ ██╔╝██║     ██║     ██╔══██╗████╗ ████║██╔══██╗
██║      ╚████╔╝ ██║     ██║     ███████║██╔████╔██║███████║
██║       ╚██╔╝  ██║     ██║     ██╔══██║██║╚██╔╝██║██╔══██║
╚██████╗   ██║   ███████╗███████╗██║  ██║██║ ╚═╝ ██║██║  ██║
 ╚═════╝   ╚═╝   ╚══════╝╚══════╝╚═╝  ╚═╝╚═╝     ╚═╝╚═╝  ╚═╝
    ██████╗ ██████╗ ███╗   ███╗██████╗ ██╗   ██╗███████╗███████╗
   ██╔════╝██╔═══██╗████╗ ████║██╔══██╗██║   ██║██╔════╝██╔════╝
   ██║     ██║   ██║██╔████╔██║██████╔╝██║   ██║███████╗█████╗
   ██║     ██║   ██║██║╚██╔╝██║██╔═══╝ ██║   ██║╚════██║██╔══╝
   ╚██████╗╚██████╔╝██║ ╚═╝ ██║██║     ╚██████╔╝███████║███████╗
    ╚═════╝ ╚═════╝ ╚═╝     ╚═╝╚═╝      ╚═════╝ ╚══════╝╚══════╝
```

---

## Features

- **Fully Local** — Uses [Ollama](https://ollama.com) (Llama 3) for reasoning. No cloud APIs.
- **Universal Desktop Control** — Drives ANY desktop application via PyAutoGUI.
- **OCR Perception** — Reads the screen using EasyOCR.
- **Cyberpunk TUI** — Neon-colored terminal interface built with Ink (React for CLI).
- **Safety First** — Confirmation prompts for dangerous actions; full action logging.

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    CYLLAMA COMPUSE                       │
├─────────────┬───────────────────────────┬───────────────┤
│  main.ts    │  main.py                  │  agent.py     │
│  (Node TUI) │  (Python CLI)             │  (Agent Loop) │
│             │                           │               │
│  Spawns ────┤──► Receives task ─────────┤──► Loop:      │
│  Python     │                           │   1. capture  │
│  process    │  Streams JSON events ◄────┤   2. OCR      │
│             │  to stdout                │   3. prompt   │
│  Renders    │                           │   4. Ollama   │
│  TUI from   │                           │   5. execute  │
│  events     │                           │   6. repeat   │
├─────────────┴───────────────────────────┴───────────────┤
│  computers/desktop/desktop.py                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  PyAutoGUI   │  │   EasyOCR    │  │   Pillow     │  │
│  │  (control)   │  │   (perceive) │  │   (capture)  │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
├─────────────────────────────────────────────────────────┤
│  Ollama (localhost:11434)                               │
│  └── llama3:8b (or any supported model)                 │
└─────────────────────────────────────────────────────────┘
```

---

## Quick Start

### Prerequisites

- **Python 3.10+**
- **Node.js 18+** (for the TUI)
- **Ollama** — [Install Ollama](https://ollama.com/download)

### 1. Clone the repo

```bash
git clone https://github.com/djmahe4/cyllama-compuse.git
cd cyllama-compuse
```

### 2. Install Python dependencies

```bash
python -m venv .venv
source .venv/bin/activate   # or .venv\Scripts\activate on Windows
pip install -r requirements.txt
```

### 3. Install Node.js dependencies

```bash
npm install
npm run build
```

### 4. Pull the Ollama model

```bash
ollama pull llama3:8b
```

### 5. Configure environment

```bash
cp .env.example .env
# Edit .env if needed
```

---

## Usage

### Python CLI (direct)

```bash
python main.py "open the calculator app and compute 42 * 17"
```

```bash
python main.py --interactive
```

### Node.js TUI (cyberpunk interface)

```bash
npm start -- "search for weather in Tokyo"
```

Or use the dev mode (no build step):

```bash
npm run dev -- "open notepad and type hello world"
```

### CLI Options

| Flag | Description | Default |
|------|-------------|---------|
| `task` | Task description (positional) | — |
| `--model` | Ollama model name | `llama3:8b` |
| `--max-iterations` | Max agent loop iterations | `30` |
| `--interactive` | Prompt for task interactively | `false` |
| `--verbose` | Enable debug logging | `false` |

---

## Example Tasks

```bash
# Open browser and search
python main.py "open Firefox and search for 'Python tutorials'"

# File management
python main.py "open the file manager and create a new folder called 'projects'"

# Text editing
python main.py "open a text editor and write a haiku about AI"

# Calculator
python main.py "open the calculator and compute 123 * 456"
```

---

## Configuration

All settings are in `.env` (see `.env.example`):

| Variable | Description | Default |
|----------|-------------|---------|
| `OLLAMA_HOST` | Ollama API endpoint | `http://localhost:11434` |
| `MODEL_NAME` | Model to use | `llama3:8b` |
| `OCR_ENGINE` | OCR backend | `easyocr` |
| `CONFIRM_DANGEROUS` | Require confirmation | `true` |
| `LOG_DIR` | Log directory | `logs` |
| `LOG_FILE` | Log filename | `agent-history.json` |

---

## Project Structure

```
cyllama-compuse/
├── .github/workflows/     # CI pipeline
├── computers/
│   ├── desktop/
│   │   └── desktop.py     # PyAutoGUI + EasyOCR desktop control
│   └── playwright/
│       └── playwright.py  # Compatibility stub
├── src/                   # TypeScript TUI components
│   ├── App.tsx
│   ├── Header.tsx
│   ├── TaskInput.tsx
│   ├── LogPanel.tsx
│   ├── OCRPanel.tsx
│   └── StatusBar.tsx
├── agent.py               # Core agent loop (Ollama)
├── main.py                # Python CLI entry point
├── main.ts                # Node.js TUI entry point
├── package.json
├── requirements.txt
├── tsconfig.json
├── .env.example
├── test_agent.py
├── test_main.py
├── README.md
├── CONTRIBUTING.md
└── LICENSE
```

---

## Event Protocol

The Python agent streams JSON events to stdout, consumed by the Node TUI:

```json
{"type": "log",    "data": {"message": "Starting task: ..."}}
{"type": "ocr",    "data": {"elements": [{"text": "OK", "x": 100, "y": 200, "w": 40, "h": 20, "confidence": 0.95}]}}
{"type": "action", "data": {"action": "click", "x": 100, "y": 200, "reason": "clicking OK button"}}
{"type": "done",   "data": {"iterations": 5, "task": "..."}}
```

Dangerous actions also emit a **flat** confirmation-request event (not nested under `data`):

```json
{
  "type": "confirmation_request",
  "action": {"action": "type", "text": "...", "reason": "..."},
  "message": "⚠️ Dangerous action: type — confirm? [y/N]",
  "timestamp": 1712345678.9
}
```

The TUI/CLI must then write one of the following to the agent's **stdin** to unblock it:

```
CONFIRM:y    ← approve
CONFIRM:n    ← deny (default if nothing is received within 30 s)
```

---

## Safety Protocol

### How it works

All dangerous actions require **explicit confirmation** before they are executed.
The agent never silently proceeds — it defaults to **deny** on any ambiguity.

```
  Python agent (agent.py)              Node TUI (main.ts / App.tsx)
  ──────────────────────────           ──────────────────────────────
  detects dangerous action
        │
        ▼
  print confirmation_request  ──────►  parse event from stdout
  event to stdout                       │
        │                               ▼
        │                         show ConfirmationModal
  block on stdin.readline()      (or prompt in plain mode)
        │                               │
        │         CONFIRM:y / CONFIRM:n │
        ◄───────────────────────────────┘
        │
        ▼
  approved? → execute  /  denied? → continue loop, inform model
```

### Dangerous actions

The following action types trigger a confirmation prompt:

| Action | Description |
|--------|-------------|
| `type` | Types text via keyboard |
| `hotkey` | Presses a key combination |
| `type_text` | Alias used by some model outputs |
| `press_key` | Single key press |
| `double_click` | Double-click (can trigger launchers) |
| `right_click` | Context-menu click |

### CLI mode (TTY)

When running directly from a terminal (`python main.py`), the prompt appears on
**stderr** (keeping stdout clean for JSON events) and the user types `y` or `n`:

```
⚠️  Dangerous action: {"action": "type", "text": "hello"}
Proceed? [y/N]
```

### TUI mode (Node.js)

When spawned by `main.ts`, a **ConfirmationModal** overlay appears:

```
╔══════════════════════════════════════════════╗
║   ⚠ ⚠ ⚠  DANGEROUS ACTION DETECTED  ⚠ ⚠ ⚠  ║
║  ⚠️ Dangerous action: type — confirm? [y/N]  ║
║                                              ║
║    [ Y ]  CONFIRM      [ N / ESC ]  DENY     ║
╚══════════════════════════════════════════════╝
```

The modal captures keyboard input; `Y` confirms, `N` or `Esc` denies.

### Timeout

If no response is received within **30 seconds** the action is **automatically denied**.

### Logging

Every confirmation decision (approved or denied) is appended to
`logs/agent-history.json`:

```json
{
  "event": "safety_decision",
  "action": {"action": "type", "text": "hello"},
  "approved": false,
  "timestamp": "2026-04-15T05:00:00Z"
}
```

### Disable confirmations

Set `CONFIRM_DANGEROUS=false` in `.env` to skip prompts entirely (not recommended
for production use):

```bash
CONFIRM_DANGEROUS=false python main.py "my task"
```

---

## Running Tests

```bash
pytest test_agent.py test_main.py -v
```

---

## License

Apache 2.0 — see [LICENSE](LICENSE).
