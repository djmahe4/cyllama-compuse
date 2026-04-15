#!/usr/bin/env node
/**
 * CYLLAMA COMPUSE — Node.js entry point
 *
 * Spawns the Python agent process and feeds JSON events into the Ink TUI.
 * If Ink / React are not installed the events are printed as plain JSON.
 */

import { spawn, type ChildProcess } from "node:child_process";
import * as readline from "node:readline";
import * as path from "node:path";

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

export interface AgentEvent {
  type: "log" | "action" | "ocr" | "done";
  data: Record<string, unknown>;
}

// ---------------------------------------------------------------------------
// Python process management
// ---------------------------------------------------------------------------

export function spawnAgent(task: string, model?: string): ChildProcess {
  const args = [path.resolve("main.py"), task];
  if (model) {
    args.push("--model", model);
  }
  const child = spawn("python3", args, {
    stdio: ["pipe", "pipe", "pipe"],
    env: { ...process.env },
  });
  return child;
}

function parseEvent(line: string): AgentEvent | null {
  try {
    const obj = JSON.parse(line.trim());
    if (obj && typeof obj.type === "string" && obj.data) {
      return obj as AgentEvent;
    }
  } catch {
    // ignore non-JSON lines
  }
  return null;
}

// ---------------------------------------------------------------------------
// TUI renderer (tries Ink, falls back to plain JSON)
// ---------------------------------------------------------------------------

async function renderWithInk(task: string, model?: string): Promise<void> {
  // Dynamic import so the module is optional
  const { render } = await import("ink");
  const React = await import("react");
  const { default: App } = await import("./src/App.js");

  const child = spawnAgent(task, model);

  const { waitUntilExit } = render(
    React.createElement(App, { task, agentProcess: child })
  );

  await waitUntilExit();
}

async function renderPlain(task: string, model?: string): Promise<void> {
  const child = spawnAgent(task, model);

  if (!child.stdout) {
    console.error("Failed to get stdout from agent process");
    process.exit(1);
  }

  const rl = readline.createInterface({ input: child.stdout });

  console.log(`\n  ╔══════════════════════════════════════╗`);
  console.log(`  ║   C Y L L A M A   C O M P U S E     ║`);
  console.log(`  ╚══════════════════════════════════════╝\n`);
  console.log(`  Task: ${task}\n`);

  for await (const line of rl) {
    const event = parseEvent(line);
    if (event) {
      switch (event.type) {
        case "log":
          console.log(`  [LOG] ${(event.data as { message?: string }).message ?? ""}`);
          break;
        case "action":
          console.log(`  [ACT] ${JSON.stringify(event.data)}`);
          break;
        case "ocr":
          console.log(
            `  [OCR] ${((event.data as { elements?: unknown[] }).elements ?? []).length} elements detected`
          );
          break;
        case "done":
          console.log(`  [DONE] Agent finished.`);
          break;
      }
    }
  }

  child.stderr?.on("data", (buf: Buffer) => {
    process.stderr.write(buf);
  });

  await new Promise<void>((resolve) => child.on("close", () => resolve()));
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

async function main(): Promise<void> {
  const args = process.argv.slice(2);
  const task = args.filter((a) => !a.startsWith("--"))[0];
  const modelIdx = args.indexOf("--model");
  const model = modelIdx !== -1 ? args[modelIdx + 1] : undefined;

  if (!task) {
    console.error("Usage: cyllama-compuse <task> [--model <name>]");
    process.exit(1);
  }

  try {
    await renderWithInk(task, model);
  } catch {
    // Ink not available — fall back to plain output
    await renderPlain(task, model);
  }
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
