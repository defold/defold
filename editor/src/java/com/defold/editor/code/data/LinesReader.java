package com.defold.editor.code.data;

import java.io.IOException;
import java.io.Reader;
import java.nio.CharBuffer;
import java.util.Collections;
import java.util.List;

public class LinesReader extends Reader {
    private final List<String> lines;
    private int row;
    private int col;
    private boolean closed;

    public LinesReader(List<String> lines) {
        this.lines = lines == null ? Collections.emptyList() : lines;
    }

    @Override
    public void close() {
        closed = true;
    }

    @Override
    public int read() throws IOException {
        char[] buffer = new char[1];
        int n = read(buffer, 0, 1);
        return (n == -1) ? -1 : buffer[0];
    }

    @Override
    public int read(char[] destBuffer) throws IOException {
        return read(destBuffer, 0, destBuffer.length);
    }

    @Override
    public int read(CharBuffer target) throws IOException {
        char[] buffer = new char[target.remaining()];
        int numRead = read(buffer, 0, buffer.length);
        if (numRead > 0) {
            target.put(buffer, 0, numRead);
        }
        return numRead;
    }

    @Override
    public int read(char[] destBuffer, int destOffset, int length) throws IOException {
        if (closed) {
            throw new IOException("Cannot read from a closed LinesReader.");
        }

        int remaining = length;
        while (true) {
            // End of stream
            if (row >= lines.size()) {
                return (remaining == length) ? -1 : length - remaining;
            }

            String line = lines.get(row);

            // End of last line
            if (row == lines.size() - 1 && col == line.length()) {
                row++;
                col = 0;
                return (remaining == length) ? -1 : length - remaining;
            }

            // End of current line → emit newline
            if (col == line.length()) {
                if (remaining == 0) {
                    return length - remaining;
                }
                destBuffer[destOffset++] = '\n';
                remaining--;
                row++;
                col = 0;
                continue;
            }

            // Copy characters from line
            int lineRemaining = line.length() - col;
            int numCopied = Math.min(remaining, lineRemaining);
            int endCol = col + numCopied;
            line.getChars(col, endCol, destBuffer, destOffset);
            destOffset += numCopied;
            remaining -= numCopied;
            col = endCol;

            // Buffer full
            if (remaining == 0) {
                return length;
            }
        }
    }
}
