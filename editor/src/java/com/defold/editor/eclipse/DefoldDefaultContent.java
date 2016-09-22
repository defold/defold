package com.defold.editor.eclipse;

import java.util.Vector;

import org.eclipse.fx.ui.controls.styledtext.StyledTextContent;
import org.eclipse.fx.ui.controls.styledtext.TextChangedEvent;
import org.eclipse.fx.ui.controls.styledtext.TextChangingEvent;

class DefoldDefaultContent implements StyledTextContent {
	static class Compatibility {
		public static int pow2(int n) {
			if (n >= 1 && n <= 30)
				return 2 << (n - 1);
			else if (n != 0) {
				throw new IllegalArgumentException();
			}
			return 1;
		}
	}

	static class SWT {
		public static final char CR = '\r';
		public static final char LF = '\n';
	}

	private final static String LineDelimiter = System.getProperty("line.separator"); //$NON-NLS-1$

	Vector<TextChangeListener> textListeners = new Vector<>(); // stores text
																// listeners for
																// event sending
	char[] textStore = new char[0]; // stores the actual text
	int gapStart = -1; // the character position start of the gap
	int gapEnd = -1; // the character position after the end of the gap
	int gapLine = -1; // the line on which the gap exists, the gap will always
						// be associated with one line
	int highWatermark = 300;
	int lowWatermark = 50;

	int[][] lines = new int[50][2]; // array of character positions and lengths
									// representing the lines of text
	int lineCount = 0; // the number of lines of text
	int expandExp = 1; // the expansion exponent, used to increase the lines
						// array exponentially
	int replaceExpandExp = 1; // the expansion exponent, used to increase the
								// lines array exponentially

	/**
	 * Creates a new DefaultContent and initializes it. A
	 * <code>StyledTextContent</> will always have
	 * at least one empty line.
	 */
	DefoldDefaultContent() {
		super();
		setText(""); //$NON-NLS-1$
	}

	/**
	 * Adds a line to the end of the line indexes array. Increases the size of
	 * the array if necessary. <code>lineCount</code> is updated to reflect the
	 * new entry.
	 * <p>
	 *
	 * @param start
	 *            the start of the line
	 * @param length
	 *            the length of the line
	 */
	void addLineIndex(int start, int length) {
		int size = this.lines.length;
		if (this.lineCount == size) {
			// expand the lines by powers of 2
			int[][] newLines = new int[size + Compatibility.pow2(this.expandExp)][2];
			System.arraycopy(this.lines, 0, newLines, 0, size);
			this.lines = newLines;
			this.expandExp++;
		}
		int[] range = new int[] { start, length };
		this.lines[this.lineCount] = range;
		this.lineCount++;
	}

	/**
	 * Adds a line index to the end of <code>linesArray</code>. Increases the
	 * size of the array if necessary and returns a new array.
	 * <p>
	 *
	 * @param start
	 *            the start of the line
	 * @param length
	 *            the length of the line
	 * @param linesArray
	 *            the array to which to add the line index
	 * @param count
	 *            the position at which to add the line
	 * @return a new array of line indexes
	 */
	int[][] addLineIndex(int start, int length, int[][] linesArray, int count) {
		int size = linesArray.length;
		int[][] newLines = linesArray;
		if (count == size) {
			newLines = new int[size + Compatibility.pow2(this.replaceExpandExp)][2];
			this.replaceExpandExp++;
			System.arraycopy(linesArray, 0, newLines, 0, size);
		}
		int[] range = new int[] { start, length };
		newLines[count] = range;
		return newLines;
	}

	@Override
	public void addTextChangeListener(TextChangeListener listener) {
		if (listener == null)
			throw new IllegalArgumentException();

		this.textListeners.addElement(listener);
	}

	/**
	 * Adjusts the gap to accommodate a text change that is occurring.
	 * <p>
	 *
	 * @param position
	 *            the position at which a change is occurring
	 * @param sizeHint
	 *            the size of the change
	 * @param line
	 *            the line where the gap will go
	 */
	void adjustGap(int position, int sizeHint, int line) {
		if (position == this.gapStart) {
			// text is being inserted at the gap position
			int size = (this.gapEnd - this.gapStart) - sizeHint;
			if (this.lowWatermark <= size && size <= this.highWatermark)
				return;
		} else if ((position + sizeHint == this.gapStart) && (sizeHint < 0)) {
			// text is being deleted at the gap position
			int size = (this.gapEnd - this.gapStart) - sizeHint;
			if (this.lowWatermark <= size && size <= this.highWatermark)
				return;
		}
		moveAndResizeGap(position, sizeHint, line);
	}

	/**
	 * Calculates the indexes of each line in the text store. Assumes no gap
	 * exists. Optimized to do less checking.
	 */
	void indexLines() {
		int start = 0;
		this.lineCount = 0;
		int textLength = this.textStore.length;
		int i;
		for (i = start; i < textLength; i++) {
			char ch = this.textStore[i];
			if (ch == SWT.CR) {
				// see if the next character is a LF
				if (i + 1 < textLength) {
					ch = this.textStore[i + 1];
					if (ch == SWT.LF) {
						i++;
					}
				}
				addLineIndex(start, i - start + 1);
				start = i + 1;
			} else if (ch == SWT.LF) {
				addLineIndex(start, i - start + 1);
				start = i + 1;
			}
		}
		addLineIndex(start, i - start);
	}

	/**
	 * Returns whether or not the given character is a line delimiter. Both CR
	 * and LF are valid line delimiters.
	 * <p>
	 *
	 * @param ch
	 *            the character to test
	 * @return true if ch is a delimiter, false otherwise
	 */
	static boolean isDelimiter(char ch) {
		if (ch == SWT.CR)
			return true;
		if (ch == SWT.LF)
			return true;
		return false;
	}

	/**
	 * Determine whether or not the replace operation is valid. DefaultContent
	 * will not allow the /r/n line delimiter to be split or partially deleted.
	 * <p>
	 *
	 * @param start
	 *            start offset of text to replace
	 * @param replaceLength
	 *            start offset of text to replace
	 * @param newText
	 *            start offset of text to replace
	 * @return a boolean specifying whether or not the replace operation is
	 *         valid
	 */
	protected boolean isValidReplace(int start, int replaceLength, String newText) {
		if (replaceLength == 0) {
			// inserting text, see if the \r\n line delimiter is being split
			if (start == 0)
				return true;
			if (start == getCharCount())
				return true;
			char before = getTextRange(start - 1, 1).charAt(0);
			if (before == '\r') {
				char after = getTextRange(start, 1).charAt(0);
				if (after == '\n')
					return false;
			}
		} else {
			// deleting text, see if part of a \r\n line delimiter is being
			// deleted
			char startChar = getTextRange(start, 1).charAt(0);
			if (startChar == '\n') {
				// see if char before delete position is \r
				if (start != 0) {
					char before = getTextRange(start - 1, 1).charAt(0);
					if (before == '\r')
						return false;
				}
			}
			char endChar = getTextRange(start + replaceLength - 1, 1).charAt(0);
			if (endChar == '\r') {
				// see if char after delete position is \n
				if (start + replaceLength != getCharCount()) {
					char after = getTextRange(start + replaceLength, 1).charAt(0);
					if (after == '\n')
						return false;
				}
			}
		}
		return true;
	}

	/**
	 * Calculates the indexes of each line of text in the given range.
	 * <p>
	 *
	 * @param offset
	 *            the logical start offset of the text lineate
	 * @param length
	 *            the length of the text to lineate, includes gap
	 * @param numLines
	 *            the number of lines to initially allocate for the line index
	 *            array, passed in for efficiency (the exact number of lines may
	 *            be known)
	 * @return a line indexes array where each line is identified by a start
	 *         offset and a length
	 */
	int[][] indexLines(int offset, int length, int numLines) {
		int[][] indexedLines = new int[numLines][2];
		int start = 0;
		int lineCount = 0;
		int i;
		this.replaceExpandExp = 1;
		for (i = start; i < length; i++) {
			int location = i + offset;
			if ((location >= this.gapStart) && (location < this.gapEnd)) {
				// ignore the gap
			} else {
				char ch = this.textStore[location];
				if (ch == SWT.CR) {
					// see if the next character is a LF
					if (location + 1 < this.textStore.length) {
						ch = this.textStore[location + 1];
						if (ch == SWT.LF) {
							i++;
						}
					}
					indexedLines = addLineIndex(start, i - start + 1, indexedLines, lineCount);
					lineCount++;
					start = i + 1;
				} else if (ch == SWT.LF) {
					indexedLines = addLineIndex(start, i - start + 1, indexedLines, lineCount);
					lineCount++;
					start = i + 1;
				}
			}
		}
		int[][] newLines = new int[lineCount + 1][2];
		System.arraycopy(indexedLines, 0, newLines, 0, lineCount);
		int[] range = new int[] { start, i - start };
		newLines[lineCount] = range;
		return newLines;
	}

	/**
	 * Inserts text.
	 * <p>
	 *
	 * @param position
	 *            the position at which to insert the text
	 * @param text
	 *            the text to insert
	 */
	void insert(int position, String text) {
		if (text.length() == 0)
			return;

		int startLine = getLineAtOffset(position);
		int change = text.length();
		boolean endInsert = position == getCharCount();
		adjustGap(position, change, startLine);

		// during an insert the gap will be adjusted to start at
		// position and it will be associated with startline, the
		// inserted text will be placed in the gap
		int startLineOffset = getOffsetAtLine(startLine);
		// at this point, startLineLength will include the start line
		// and all of the newly inserted text
		int startLineLength = getPhysicalLine(startLine).length();

		if (change > 0) {
			// shrink gap
			this.gapStart += (change);
			for (int i = 0; i < text.length(); i++) {
				this.textStore[position + i] = text.charAt(i);
			}
		}

		// figure out the number of new lines that have been inserted
		int[][] newLines = indexLines(startLineOffset, startLineLength, 10);
		// only insert an empty line if it is the last line in the text
		int numNewLines = newLines.length - 1;
		if (newLines[numNewLines][1] == 0) {
			// last inserted line is a new line
			if (endInsert) {
				// insert happening at end of the text, leave numNewLines as
				// is since the last new line will not be concatenated with
				// another
				// line
				numNewLines += 1;
			} else {
				numNewLines -= 1;
			}
		}

		// make room for the new lines
		expandLinesBy(numNewLines);
		// shift down the lines after the replace line
		for (int i = this.lineCount - 1; i > startLine; i--) {
			this.lines[i + numNewLines] = this.lines[i];
		}
		// insert the new lines
		for (int i = 0; i < numNewLines; i++) {
			newLines[i][0] += startLineOffset;
			this.lines[startLine + i] = newLines[i];
		}
		// update the last inserted line
		if (numNewLines < newLines.length) {
			newLines[numNewLines][0] += startLineOffset;
			this.lines[startLine + numNewLines] = newLines[numNewLines];
		}

		this.lineCount += numNewLines;
		this.gapLine = getLineAtPhysicalOffset(this.gapStart);
	}

	/**
	 * Moves the gap and adjusts its size in anticipation of a text change. The
	 * gap is resized to actual size + the specified size and moved to the given
	 * position.
	 * <p>
	 *
	 * @param position
	 *            the position at which a change is occurring
	 * @param size
	 *            the size of the change
	 * @param newGapLine
	 *            the line where the gap should be put
	 */
	void moveAndResizeGap(int position, int size, int newGapLine) {
		char[] content = null;
		int oldSize = this.gapEnd - this.gapStart;
		int newSize;
		if (size > 0) {
			newSize = this.highWatermark + size;
		} else {
			newSize = this.lowWatermark - size;
		}
		// remove the old gap from the lines information
		if (gapExists()) {
			// adjust the line length
			this.lines[this.gapLine][1] = this.lines[this.gapLine][1] - oldSize;
			// adjust the offsets of the lines after the gapLine
			for (int i = this.gapLine + 1; i < this.lineCount; i++) {
				this.lines[i][0] = this.lines[i][0] - oldSize;
			}
		}

		if (newSize < 0) {
			if (oldSize > 0) {
				// removing the gap
				content = new char[this.textStore.length - oldSize];
				System.arraycopy(this.textStore, 0, content, 0, this.gapStart);
				System.arraycopy(this.textStore, this.gapEnd, content, this.gapStart, content.length - this.gapStart);
				this.textStore = content;
			}
			this.gapStart = this.gapEnd = position;
			return;
		}
		content = new char[this.textStore.length + (newSize - oldSize)];
		int newGapStart = position;
		int newGapEnd = newGapStart + newSize;
		if (oldSize == 0) {
			System.arraycopy(this.textStore, 0, content, 0, newGapStart);
			System.arraycopy(this.textStore, newGapStart, content, newGapEnd, content.length - newGapEnd);
		} else if (newGapStart < this.gapStart) {
			int delta = this.gapStart - newGapStart;
			System.arraycopy(this.textStore, 0, content, 0, newGapStart);
			System.arraycopy(this.textStore, newGapStart, content, newGapEnd, delta);
			System.arraycopy(this.textStore, this.gapEnd, content, newGapEnd + delta, this.textStore.length - this.gapEnd);
		} else {
			int delta = newGapStart - this.gapStart;
			System.arraycopy(this.textStore, 0, content, 0, this.gapStart);
			System.arraycopy(this.textStore, this.gapEnd, content, this.gapStart, delta);
			System.arraycopy(this.textStore, this.gapEnd + delta, content, newGapEnd, content.length - newGapEnd);
		}
		this.textStore = content;
		this.gapStart = newGapStart;
		this.gapEnd = newGapEnd;

		// add the new gap to the lines information
		if (gapExists()) {
			this.gapLine = newGapLine;
			// adjust the line length
			int gapLength = this.gapEnd - this.gapStart;
			this.lines[this.gapLine][1] = this.lines[this.gapLine][1] + (gapLength);
			// adjust the offsets of the lines after the gapLine
			for (int i = this.gapLine + 1; i < this.lineCount; i++) {
				this.lines[i][0] = this.lines[i][0] + gapLength;
			}
		}
	}

	/**
	 * Returns the number of lines that are in the specified text.
	 * <p>
	 *
	 * @param startOffset
	 *            the start of the text to lineate
	 * @param length
	 *            the length of the text to lineate
	 * @return number of lines
	 */
	int lineCount(int startOffset, int length) {
		if (length == 0) {
			return 0;
		}
		int lineCount = 0;
		int count = 0;
		int i = startOffset;
		if (i >= this.gapStart) {
			i += this.gapEnd - this.gapStart;
		}
		while (count < length) {
			if ((i >= this.gapStart) && (i < this.gapEnd)) {
				// ignore the gap
			} else {
				char ch = this.textStore[i];
				if (ch == SWT.CR) {
					// see if the next character is a LF
					if (i + 1 < this.textStore.length) {
						ch = this.textStore[i + 1];
						if (ch == SWT.LF) {
							i++;
							count++;
						}
					}
					lineCount++;
				} else if (ch == SWT.LF) {
					lineCount++;
				}
				count++;
			}
			i++;
		}
		return lineCount;
	}

	/**
	 * Returns the number of lines that are in the specified text.
	 * <p>
	 *
	 * @param text
	 *            the text to lineate
	 * @return number of lines in the text
	 */
	static int lineCount(String text) {
		int lineCount = 0;
		int length = text.length();
		for (int i = 0; i < length; i++) {
			char ch = text.charAt(i);
			if (ch == SWT.CR) {
				if (i + 1 < length && text.charAt(i + 1) == SWT.LF) {
					i++;
				}
				lineCount++;
			} else if (ch == SWT.LF) {
				lineCount++;
			}
		}
		return lineCount;
	}

	/**
	 * @return the logical length of the text store
	 */
	@Override
	public int getCharCount() {
		int length = this.gapEnd - this.gapStart;
		return (this.textStore.length - length);
	}

	/**
	 * Returns the line at <code>index</code> without delimiters.
	 * <p>
	 *
	 * @param index
	 *            the index of the line to return
	 * @return the logical line text (i.e., without the gap)
	 * @exception IllegalArgumentException
	 *                <ul>
	 *                <li>ERROR_INVALID_ARGUMENT when index is out of range</li>
	 *                </ul>
	 */
	@SuppressWarnings("null")
	@Override
	public String getLine(int index) {
		if ((index >= this.lineCount) || (index < 0))
			throw new IllegalArgumentException();
		int start = this.lines[index][0];
		int length = this.lines[index][1];
		int end = start + length - 1;
		if (!gapExists() || (end < this.gapStart) || (start >= this.gapEnd)) {
			// line is before or after the gap
			while ((length - 1 >= 0) && isDelimiter(this.textStore[start + length - 1])) {
				length--;
			}
			return new String(this.textStore, start, length);
		} else {
			// gap is in the specified range, strip out the gap
			StringBuffer buf = new StringBuffer();
			int gapLength = this.gapEnd - this.gapStart;
			buf.append(this.textStore, start, this.gapStart - start);
			buf.append(this.textStore, this.gapEnd, length - gapLength - (this.gapStart - start));
			length = buf.length();
			while ((length - 1 >= 0) && isDelimiter(buf.charAt(length - 1))) {
				length--;
			}
			return buf.toString().substring(0, length);
		}
	}

	/**
	 * Returns the line delimiter that should be used by the StyledText widget
	 * when inserting new lines. This delimiter may be different than the
	 * delimiter that is used by the <code>StyledTextContent</code> interface.
	 * <p>
	 *
	 * @return the platform line delimiter as specified in the line.separator
	 *         system property.
	 */
	public static String getLineDelimiter() {
		return LineDelimiter;
	}

	/**
	 * Returns the line at the given index with delimiters.
	 * <p>
	 *
	 * @param index
	 *            the index of the line to return
	 * @return the logical line text (i.e., without the gap) with delimiters
	 */
	String getFullLine(int index) {
		int start = this.lines[index][0];
		int length = this.lines[index][1];
		int end = start + length - 1;
		if (!gapExists() || (end < this.gapStart) || (start >= this.gapEnd)) {
			// line is before or after the gap
			return new String(this.textStore, start, length);
		} else {
			// gap is in the specified range, strip out the gap
			StringBuffer buffer = new StringBuffer();
			int gapLength = this.gapEnd - this.gapStart;
			buffer.append(this.textStore, start, this.gapStart - start);
			buffer.append(this.textStore, this.gapEnd, length - gapLength - (this.gapStart - start));
			return buffer.toString();
		}
	}

	/**
	 * Returns the physical line at the given index (i.e., with delimiters and
	 * the gap).
	 * <p>
	 *
	 * @param index
	 *            the line index
	 * @return the physical line
	 */
	String getPhysicalLine(int index) {
		int start = this.lines[index][0];
		int length = this.lines[index][1];
		return getPhysicalText(start, length);
	}

	/**
	 * @return the number of lines in the text store
	 */
	@Override
	public int getLineCount() {
		return this.lineCount;
	}

	/**
	 * Returns the line at the given offset.
	 * <p>
	 *
	 * @param charPosition
	 *            logical character offset (i.e., does not include gap)
	 * @return the line index
	 * @exception IllegalArgumentException
	 *                <ul>
	 *                <li>ERROR_INVALID_ARGUMENT when charPosition is out of
	 *                range</li>
	 *                </ul>
	 */
	@Override
	public int getLineAtOffset(int charPosition) {
		if ((charPosition > getCharCount()) || (charPosition < 0))
			throw new IllegalArgumentException();
		int position;
		if (charPosition < this.gapStart) {
			// position is before the gap
			position = charPosition;
		} else {
			// position includes the gap
			position = charPosition + (this.gapEnd - this.gapStart);
		}

		// if last line and the line is not empty you can ask for
		// a position that doesn't exist (the one to the right of the
		// last character) - for inserting
		if (this.lineCount > 0) {
			int lastLine = this.lineCount - 1;
			if (position == this.lines[lastLine][0] + this.lines[lastLine][1])
				return lastLine;
		}

		int high = this.lineCount;
		int low = -1;
		int index = this.lineCount;
		while (high - low > 1) {
			index = (high + low) / 2;
			int lineStart = this.lines[index][0];
			int lineEnd = lineStart + this.lines[index][1] - 1;
			if (position <= lineStart) {
				high = index;
			} else if (position <= lineEnd) {
				high = index;
				break;
			} else {
				low = index;
			}
		}
		return high;
	}

	/**
	 * Returns the line index at the given physical offset.
	 * <p>
	 *
	 * @param position
	 *            physical character offset (i.e., includes gap)
	 * @return the line index
	 */
	int getLineAtPhysicalOffset(int position) {
		int high = this.lineCount;
		int low = -1;
		int index = this.lineCount;
		while (high - low > 1) {
			index = (high + low) / 2;
			int lineStart = this.lines[index][0];
			int lineEnd = lineStart + this.lines[index][1] - 1;
			if (position <= lineStart) {
				high = index;
			} else if (position <= lineEnd) {
				high = index;
				break;
			} else {
				low = index;
			}
		}
		return high;
	}

	/**
	 * Returns the logical offset of the given line.
	 * <p>
	 *
	 * @param lineIndex
	 *            index of line
	 * @return the logical starting offset of the line. When there are not any
	 *         lines, getOffsetAtLine(0) is a valid call that should answer 0.
	 * @exception IllegalArgumentException
	 *                <ul>
	 *                <li>ERROR_INVALID_ARGUMENT when lineIndex is out of range
	 *                </li>
	 *                </ul>
	 */
	@Override
	public int getOffsetAtLine(int lineIndex) {
		if (lineIndex == 0)
			return 0;
		if ((lineIndex >= this.lineCount) || (lineIndex < 0))
			throw new IllegalArgumentException();
		int start = this.lines[lineIndex][0];
		if (start > this.gapEnd) {
			return start - (this.gapEnd - this.gapStart);
		} else {
			return start;
		}
	}

	/**
	 * Increases the line indexes array to accommodate more lines.
	 * <p>
	 *
	 * @param numLines
	 *            the number to increase the array by
	 */
	void expandLinesBy(int numLines) {
		int size = this.lines.length;
		if (size - this.lineCount >= numLines) {
			return;
		}
		int[][] newLines = new int[size + Math.max(10, numLines)][2];
		System.arraycopy(this.lines, 0, newLines, 0, size);
		this.lines = newLines;
	}

	// /**
	// * Reports an SWT error.
	// * <p>
	// *
	// * @param code the error code
	// */
	// void error (int code) {
	// SWT.error(code);
	// }
	/**
	 * Returns whether or not a gap exists in the text store.
	 * <p>
	 *
	 * @return true if gap exists, false otherwise
	 */
	boolean gapExists() {
		return this.gapStart != this.gapEnd;
	}

	/**
	 * Returns a string representing the continuous content of the text store.
	 * <p>
	 *
	 * @param start
	 *            the physical start offset of the text to return
	 * @param length
	 *            the physical length of the text to return
	 * @return the text
	 */
	String getPhysicalText(int start, int length) {
		return new String(this.textStore, start, length);
	}

	/**
	 * Returns a string representing the logical content of the text store
	 * (i.e., gap stripped out).
	 * <p>
	 *
	 * @param start
	 *            the logical start offset of the text to return
	 * @param length
	 *            the logical length of the text to return
	 * @return the text
	 */
	@SuppressWarnings("null")
	@Override
	public String getTextRange(int start, int length) {
		if (this.textStore == null)
			return ""; //$NON-NLS-1$
		if (length == 0)
			return ""; //$NON-NLS-1$
		int end = start + length;
		if (!gapExists() || (end < this.gapStart))
			return new String(this.textStore, start, length);
		if (this.gapStart < start) {
			int gapLength = this.gapEnd - this.gapStart;
			return new String(this.textStore, start + gapLength, length);
		}
		StringBuffer buf = new StringBuffer();
		buf.append(this.textStore, start, this.gapStart - start);
		buf.append(this.textStore, this.gapEnd, end - this.gapStart);
		return buf.toString();
	}

	/**
	 * Removes the specified <code>TextChangeListener</code>.
	 * <p>
	 *
	 * @param listener
	 *            the listener which should no longer be notified
	 *
	 * @exception IllegalArgumentException
	 *                <ul>
	 *                <li>ERROR_NULL_ARGUMENT when listener is null</li>
	 *                </ul>
	 */
	@Override
	public void removeTextChangeListener(TextChangeListener listener) {
		if (listener == null) {
			throw new IllegalArgumentException();
		}

		this.textListeners.remove(listener);
	}

	/**
	 * Replaces the text with <code>newText</code> starting at position
	 * <code>start</code> for a length of <code>replaceLength</code>. Notifies
	 * the appropriate listeners.
	 * <p>
	 *
	 * When sending the TextChangingEvent, <code>newLineCount</code> is the
	 * number of lines that are going to be inserted and
	 * <code>replaceLineCount</code> is the number of lines that are going to be
	 * deleted, based on the change that occurs visually. For example:
	 * <ul>
	 * <li>(replaceText,newText) ==> (replaceLineCount,newLineCount)
	 * <li>("","\n") ==> (0,1)
	 * <li>("\n\n","a") ==> (2,0)
	 * </ul>
	 * </p>
	 *
	 * @param start
	 *            start offset of text to replace
	 * @param replaceLength
	 *            start offset of text to replace
	 * @param newText
	 *            start offset of text to replace
	 *
	 */
	@Override
	public void replaceTextRange(int start, int replaceLength, String newText) {
		// check for invalid replace operations
		if (!isValidReplace(start, replaceLength, newText))
			throw new IllegalArgumentException();

		// inform listeners
		TextChangingEvent event = TextChangingEvent.textChanging(this, start, replaceLength, lineCount(start, replaceLength), newText, newText.length(), lineCount(newText));
		for (TextChangeListener l : this.textListeners) {
			l.textChanging(event);
		}

		// first delete the text to be replaced
		delete(start, replaceLength, event.replaceLineCount + 1);
		// then insert the new text
		insert(start, newText);
		// inform listeners
		TextChangedEvent textChanged = TextChangedEvent.textChanged(this);

		for (TextChangeListener l : this.textListeners) {
			l.textChanged(textChanged);
		}
		System.err.println(getTextRange(0, getCharCount()));
	}

	/**
	 * Sets the content to text and removes the gap since there are no sensible
	 * predictions about where the next change will occur.
	 * <p>
	 *
	 * @param text
	 *            the text
	 */
	@Override
	public void setText(String text) {
		this.textStore = text.toCharArray();
		this.gapStart = -1;
		this.gapEnd = -1;
		this.expandExp = 1;
		indexLines();

		TextChangedEvent textSet = TextChangedEvent.textSet(this);

		for (TextChangeListener l : this.textListeners) {
			l.textSet(textSet);
		}
	}

	/**
	 * Deletes text.
	 * <p>
	 *
	 * @param position
	 *            the position at which the text to delete starts
	 * @param length
	 *            the length of the text to delete
	 * @param numLines
	 *            the number of lines that are being deleted
	 */
	void delete(int position, int length, int numLines) {
		if (length == 0)
			return;

		int startLine = getLineAtOffset(position);
		int startLineOffset = getOffsetAtLine(startLine);
		int endLine = getLineAtOffset(position + length);

		String endText = ""; //$NON-NLS-1$
		boolean splittingDelimiter = false;
		if (position + length < getCharCount()) {
			endText = getTextRange(position + length - 1, 2);
			if ((endText.charAt(0) == SWT.CR) && (endText.charAt(1) == SWT.LF)) {
				splittingDelimiter = true;
			}
		}

		adjustGap(position + length, -length, startLine);
		int[][] oldLines = indexLines(position, length + (this.gapEnd - this.gapStart), numLines);

		// enlarge the gap - the gap can be enlarged either to the
		// right or left
		if (position + length == this.gapStart) {
			this.gapStart -= length;
		} else {
			this.gapEnd += length;
		}

		// figure out the length of the new concatenated line, do so by
		// finding the first line delimiter after position
		int j = position;
		boolean eol = false;
		while (j < this.textStore.length && !eol) {
			if (j < this.gapStart || j >= this.gapEnd) {
				char ch = this.textStore[j];
				if (isDelimiter(ch)) {
					if (j + 1 < this.textStore.length) {
						if (ch == SWT.CR && (this.textStore[j + 1] == SWT.LF)) {
							j++;
						}
					}
					eol = true;
				}
			}
			j++;
		}
		// update the line where the deletion started
		this.lines[startLine][1] = (position - startLineOffset) + (j - position);
		// figure out the number of lines that have been deleted
		int numOldLines = oldLines.length - 1;
		if (splittingDelimiter)
			numOldLines -= 1;
		// shift up the lines after the last deleted line, no need to update
		// the offset or length of the lines
		for (int i = endLine + 1; i < this.lineCount; i++) {
			this.lines[i - numOldLines] = this.lines[i];
		}
		this.lineCount -= numOldLines;
		this.gapLine = getLineAtPhysicalOffset(this.gapStart);
	}
}

