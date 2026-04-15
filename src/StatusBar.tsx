/**
 * CYLLAMA COMPUSE — StatusBar
 *
 * Bottom status bar showing agent state and last action.
 */

import React from "react";
import { Box, Text } from "ink";

interface StatusBarProps {
  status: string;
  iteration: number;
  lastAction: string;
}

export default function StatusBar({
  status,
  iteration,
  lastAction,
}: StatusBarProps) {
  let statusColor: string;
  let statusIcon: string;

  switch (status) {
    case "running":
      statusColor = "cyan";
      statusIcon = "⚡";
      break;
    case "done":
      statusColor = "green";
      statusIcon = "✔";
      break;
    case "exited":
      statusColor = "yellow";
      statusIcon = "◼";
      break;
    default:
      statusColor = "white";
      statusIcon = "?";
  }

  return (
    <Box
      borderStyle="single"
      borderColor="cyan"
      paddingX={1}
      justifyContent="space-between"
    >
      <Text color={statusColor} bold>
        {statusIcon} {status.toUpperCase()}
      </Text>
      <Text color="magenta">iter: {iteration}</Text>
      <Text color="green" wrap="truncate-end">
        last: {lastAction || "–"}
      </Text>
    </Box>
  );
}
