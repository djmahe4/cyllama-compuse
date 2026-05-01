# Element Engine

A hybrid UI element identification engine for cyllama-compuse that combines
platform accessibility APIs with an OCR fallback.

## What it does

The engine enumerates visible desktop UI elements using the best available
method for each platform:

| Platform | Primary API            | Fallback      |
|----------|------------------------|---------------|
| Windows  | UIAutomation (COM)     | OCR (Python)  |
| macOS    | Accessibility API (AX) | OCR (Python)  |
| Linux    | AT-SPI2                | OCR (Python)  |
| Other    | —                      | OCR (Python)  |

Each element gets a stable `ElementId` derived from its process ID, window
handle, accessibility runtime ID, and role, allowing the agent to track
elements across frames.

## Building

Prerequisites: CMake 3.16+, a C++17 compiler, and Python 3 development
headers.

```bash
cd element_engine
cmake -B build -S .
cmake --build build
```

The compiled Python extension (`element_engine_cpp*.so` / `.pyd`) will be
placed in `build/python_bindings/`.  Copy or symlink it to the project root
or to `computers/desktop/` so Python can import it.

## Using from Python

```python
from computers.desktop.element_engine_wrapper import ElementEngineSession, is_available

print("Engine available:", is_available())

with ElementEngineSession() as session:
    elements = session.enumerate(max_elements=100)
    for el in elements:
        print(el)  # dict with id, name, role_name, x, y, width, height, …
```

The `DesktopComputer.enumerate_elements()` method integrates the engine
automatically, falling back to EasyOCR when the C++ module is not compiled.

## Platform requirements

- **Windows**: Windows 10 or later; UIAutomation is built into the OS.
- **macOS**: macOS 10.14+; accessibility must be granted in
  *System Preferences → Privacy & Security → Accessibility*.
- **Linux**: AT-SPI2 (`libatspi-2.0-dev`) must be installed and the
  accessibility bus running (`at-spi2-core`).
- **CI / no display**: The OCR fallback is used automatically; no native
  accessibility APIs are required.
