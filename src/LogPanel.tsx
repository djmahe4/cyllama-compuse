/**
 * CYLLAMA COMPUSE — LogPanel
 *
 * Displays agent log messages with cyberpunk styling.
 */

import React from "react";
import { Box, Text } from "ink";

interface LogPanelProps {
  logs: string[];
}

export default function LogPanel({ logs }: LogPanelProps) {
  // Show last 15 logs
  const visible = logs.slice(-15);

  return (
    <Box
      flexDirection="column"
      borderStyle="single"
      borderColor="green"
      paddingX={1}
      height={18}
    >
      <Text color="green" bold>
        ╔══ AGENT LOG ══╗
      </Text>
      {visible.length === 0 ? (
        <Text color="gray">Waiting for agent...</Text>
      ) : (
        visible.map((msg, i) => {
          let color: string = "white";
          if (msg.includes("Iteration")) color = "cyan";
          else if (msg.includes("error") || msg.includes("Error"))
            color = "red";
          else if (msg.includes("complete") || msg.includes("done"))
            color = "green";
          else if (msg.includes("Calling")) color = "magenta";

          return (
            <Text key={i} color={color} wrap="truncate-end">
              {"▸ "}
              {msg}
            </Text>
          );
        })
      )}
    </Box>
  );
}
