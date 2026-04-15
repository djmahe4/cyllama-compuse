/**
 * CYLLAMA COMPUSE — TaskInput
 *
 * Shows the current task being executed.
 */

import React from "react";
import { Box, Text } from "ink";

interface TaskInputProps {
  task: string;
}

export default function TaskInput({ task }: TaskInputProps) {
  return (
    <Box
      borderStyle="single"
      borderColor="cyan"
      paddingX={1}
      marginBottom={1}
    >
      <Text color="cyan" bold>
        TASK:{" "}
      </Text>
      <Text color="white">{task}</Text>
    </Box>
  );
}
