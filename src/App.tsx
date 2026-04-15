/**
 * CYLLAMA COMPUSE — App (root TUI component)
 *
 * Renders the cyberpunk terminal interface.
 * Receives events from the spawned Python agent process.
 */

import React, { useState, useEffect } from "react";
import { Box, Text, useApp } from "ink";
import type { ChildProcess } from "node:child_process";
import * as readline from "node:readline";

import Header from "./Header.js";
import TaskInput from "./TaskInput.js";
import LogPanel from "./LogPanel.js";
import OCRPanel from "./OCRPanel.js";
import StatusBar from "./StatusBar.js";

interface AgentEvent {
  type: "log" | "action" | "ocr" | "done";
  data: Record<string, unknown>;
}

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

  useEffect(() => {
    if (!agentProcess.stdout) return;

    const rl = readline.createInterface({ input: agentProcess.stdout });

    const onLine = (line: string) => {
      try {
        const event: AgentEvent = JSON.parse(line.trim());

        switch (event.type) {
          case "log": {
            const msg = (event.data as { message?: string }).message ?? "";
            setLogs((prev) => [...prev.slice(-100), msg]);
            if (msg.startsWith("--- Iteration")) {
              const m = msg.match(/Iteration (\d+)/);
              if (m) setIteration(Number(m[1]));
            }
            break;
          }
          case "action":
            setLastAction(JSON.stringify(event.data));
            break;
          case "ocr":
            setOcrElements((event.data as { elements?: unknown[] }).elements ?? []);
            break;
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
    </Box>
  );
}
