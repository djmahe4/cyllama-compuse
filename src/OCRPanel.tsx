/**
 * CYLLAMA COMPUSE — OCRPanel
 *
 * Displays detected OCR elements from the screen.
 */

import React from "react";
import { Box, Text } from "ink";

interface OCRElement {
  text: string;
  x: number;
  y: number;
  w: number;
  h: number;
  confidence: number;
}

interface OCRPanelProps {
  elements: unknown[];
}

export default function OCRPanel({ elements }: OCRPanelProps) {
  const items = (elements as OCRElement[]).slice(0, 12);

  return (
    <Box
      flexDirection="column"
      borderStyle="single"
      borderColor="magenta"
      paddingX={1}
      height={18}
    >
      <Text color="magenta" bold>
        ╔══ OCR ELEMENTS ══╗
      </Text>
      {items.length === 0 ? (
        <Text color="gray">No OCR data yet...</Text>
      ) : (
        <>
          <Text color="cyan" dimColor>
            {"TEXT".padEnd(20)} {"POS".padEnd(12)} CONF
          </Text>
          {items.map((el, i) => (
            <Text key={i} color="white" wrap="truncate-end">
              {(el.text ?? "").substring(0, 18).padEnd(20)}
              {`(${el.x},${el.y})`.padEnd(12)}
              {el.confidence !== undefined
                ? (el.confidence * 100).toFixed(0) + "%"
                : "–"}
            </Text>
          ))}
        </>
      )}
      <Text color="magenta" dimColor>
        {elements.length > 12
          ? `... and ${elements.length - 12} more`
          : `${elements.length} element(s) total`}
      </Text>
    </Box>
  );
}
