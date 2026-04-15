/**
 * CYLLAMA COMPUSE — ConfirmationModal
 *
 * Cyberpunk-styled overlay that asks the user to confirm or deny a dangerous
 * action before the Python agent is allowed to execute it.
 *
 * Keys:
 *   Y       → confirm  (writes CONFIRM:y to agent stdin)
 *   N / Esc → deny     (writes CONFIRM:n to agent stdin)
 */

import React from "react";
import { Box, Text, useInput } from "ink";

export interface ConfirmationModalProps {
  /** Human-readable description of the action awaiting confirmation. */
  message: string;
  /** Called with `true` when the user confirms, `false` when they deny. */
  onConfirm: (result: boolean) => void;
}

export default function ConfirmationModal({
  message,
  onConfirm,
}: ConfirmationModalProps) {
  useInput((input, key) => {
    if (input === "y" || input === "Y") {
      onConfirm(true);
    } else if (input === "n" || input === "N" || key.escape) {
      onConfirm(false);
    }
  });

  return (
    <Box
      flexDirection="column"
      alignItems="center"
      borderStyle="double"
      borderColor="red"
      paddingX={3}
      paddingY={1}
      marginTop={1}
    >
      <Text color="red" bold>
        {"⚠ ⚠ ⚠  DANGEROUS ACTION DETECTED  ⚠ ⚠ ⚠"}
      </Text>
      <Text color="magenta">{message}</Text>
      <Text> </Text>
      <Text color="yellow" bold>
        {"  [ Y ]  CONFIRM      [ N / ESC ]  DENY  "}
      </Text>
    </Box>
  );
}
