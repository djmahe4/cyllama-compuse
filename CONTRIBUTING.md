# Contributing to CYLLAMA COMPUSE

Thank you for your interest in contributing! Here's how to get started.

## How to contribute

1. **Fork** the repository.
2. **Create a branch** for your feature or fix.
3. **Make changes** — ensure tests pass (`pytest test_agent.py test_main.py -v`).
4. **Open a Pull Request** with a clear description.

## Development setup

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
npm install
```

## Code style

- Python: follow PEP 8 conventions.
- TypeScript: standard TS strict mode.
- Keep functions small and well-documented.

## Testing

Run the test suite before submitting a PR:

```bash
pytest test_agent.py test_main.py -v
npm run build
```

## Code reviews

All submissions require review. We use GitHub pull requests for this purpose.

## Community guidelines

Be respectful, constructive, and inclusive.
