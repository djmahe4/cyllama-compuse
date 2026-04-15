/**
 * CYLLAMA COMPUSE — App (root TUI component)
 *
 * Renders the cyberpunk terminal interface.
 * Receives events from the spawned Python agent process.
 */

import React, { useState, useEffect, useCallback } from "react";
import { Box, Text, useApp } from "ink";
import type { ChildProcess } from "node:child_process";
import * as readline from "node:readline";

import Header from "./Header.js";
import TaskInput from "./TaskInput.js";
import LogPanel from "./LogPanel.js";
import OCRPanel from "./OCRPanel.js";
import StatusBar from "./StatusBar.js";
import ConfirmationModal from "./ConfirmationModal.js";

// ---------------------------------------------------------------------------
// Event types
// ---------------------------------------------------------------------------

interface StandardAgentEvent {
  type: "log" | "action" | "ocr" | "done";
  data: Record<string, unknown>;
}

interface ConfirmationRequestEvent {
  type: "confirmation_request";
  action: Record<string, unknown>;
  message: string;
  timestamp: number;
}

type AgentEvent = StandardAgentEvent | ConfirmationRequestEvent;

interface PendingConfirmation {
  message: string;
  action: Record<string, unknown>;
}

// ---------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------

interface AppProps {
  task: string;
  agentProcess: ChildProcess;
}

export default function App({ task, agentProcess }: AppProps) {
  const { exit } = useApp();
  const [logs, setLogs] = useState<string[]>([]);
  const [ocrElements, setOcrElements] = useState<unknown[]>([]);
  const [status, setStatus] = useState<string>("running");
  const [lastAction, setLastAction] = useState<string>("");
  const [iteration, setIteration] = useState(0);
  const [pendingConfirmation, setPendingConfirmation] =
    useState<PendingConfirmation | null>(null);

  /** Write CONFIRM:y or CONFIRM:n back to the agent process stdin. */
  const handleConfirmation = useCallback(
    (confirmed: boolean) => {
      setPendingConfirmation(null);
      if (agentProcess.stdin) {
        agentProcess.stdin.write((confirmed ? "CONFIRM:y" : "CONFIRM:n") + "\n");
      }
    },
    [agentProcess]
  );

  useEffect(() => {
    if (!agentProcess.stdout) return;

    const rl = readline.createInterface({ input: agentProcess.stdout });

    const onLine = (line: string) => {
      try {
        const event: AgentEvent = JSON.parse(line.trim());

        switch (event.type) {
          case "log": {
            const e = event as StandardAgentEvent;
            const msg = (e.data?.message as string) ?? "";
            setLogs((prev) => [...prev.slice(-100), msg]);
            if (msg.startsWith("--- Iteration")) {
              const m = msg.match(/Iteration (\d+)/);
              if (m) setIteration(Number(m[1]));
            }
            break;
          }
          case "action": {
            const e = event as StandardAgentEvent;
            setLastAction(JSON.stringify(e.data));
            break;
          }
          case "ocr": {
            const e = event as StandardAgentEvent;
            setOcrElements((e.data?.elements as unknown[]) ?? []);
            break;
          }
          case "confirmation_request": {
            const e = event as ConfirmationRequestEvent;
            setPendingConfirmation({
              message: e.message ?? "⚠️ Dangerous action — confirm?",
              action: e.action ?? {},
            });
            break;
          }
          case "done":
            setStatus("done");
            break;
        }
      } catch {
        // ignore non-JSON lines
      }
    };

    rl.on("line", onLine);

    agentProcess.on("close", () => {
      setStatus("exited");
      setTimeout(() => exit(), 500);
    });

    return () => {
      rl.close();
    };
  }, [agentProcess, exit]);

  return (
    <Box flexDirection="column" width="100%">
      <Header />
      <TaskInput task={task} />
      <Box flexDirection="row" width="100%">
        <Box flexDirection="column" width="60%">
          <LogPanel logs={logs} />
        </Box>
        <Box flexDirection="column" width="40%">
          <OCRPanel elements={ocrElements} />
        </Box>
      </Box>
      <StatusBar
        status={status}
        iteration={iteration}
        lastAction={lastAction}
      />
      {pendingConfirmation && (
        <ConfirmationModal
          message={pendingConfirmation.message}
          onConfirm={handleConfirmation}
        />
      )}
    </Box>
  );
}

