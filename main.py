#!/usr/bin/env python3
"""
CYLLAMA COMPUSE — Main CLI entry point

Runs the desktop automation agent and streams JSON events to stdout
so the Node.js TUI can consume them.
"""

import argparse
import json
import os
import sys
import logging

from agent import DesktopAgent


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="CYLLAMA COMPUSE — Ollama desktop automation agent"
    )
    parser.add_argument(
        "task",
        nargs="?",
        default=None,
        help="Task description for the agent to execute",
    )
    parser.add_argument(
        "--model",
        default=os.environ.get("MODEL_NAME", "llama3:8b"),
        help="Ollama model to use (default: llama3:8b)",
    )
    parser.add_argument(
        "--max-iterations",
        type=int,
        default=30,
        help="Maximum agent loop iterations (default: 30)",
    )
    parser.add_argument(
        "--interactive",
        action="store_true",
        help="Run in interactive mode (prompt for task)",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        default=False,
        help="Enable verbose logging",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    log_level = logging.DEBUG if args.verbose else logging.WARNING
    logging.basicConfig(
        level=log_level,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        stream=sys.stderr,
    )

    task = args.task
    if not task:
        if args.interactive and sys.stdin.isatty():
            task = input("Enter task: ").strip()
        else:
            # Read task from stdin (piped from Node.js)
            task = sys.stdin.readline().strip()

    if not task:
        print(
            json.dumps({"type": "log", "data": {"message": "No task provided."}}),
            flush=True,
        )
        sys.exit(1)

    agent = DesktopAgent(
        task=task,
        model=args.model,
        max_iterations=args.max_iterations,
    )

    for event in agent.run_task():
        print(json.dumps(event), flush=True)


if __name__ == "__main__":
    main()
