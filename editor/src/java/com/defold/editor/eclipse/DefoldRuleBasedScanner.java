package com.defold.editor.eclipse;

import org.eclipse.jface.text.rules.RuleBasedScanner;
import org.eclipse.jface.text.BadLocationException;
import org.eclipse.jface.text.IDocument;
import java.util.Iterator;

public class DefoldRuleBasedScanner extends RuleBasedScanner {
    public class DocumentCharSequence implements Iterable, CharSequence {
        int start;
        int end;

        public DocumentCharSequence(int start, int end) {
            this.start = start;
            this.end = end;
        }

        public char charAt(int index) {
            try {
                return fDocument.getChar(start + index);
            }
            catch (BadLocationException e) {
                System.out.println("Bad Location in Document" + "offset:" + (start + index) +  "length:" + fDocument.getLength());
	       return '\0';
            }
        }

        public int length() {
            return end - start;
        }

        public CharSequence subSequence(int subStart, int subEnd) {
            return new DocumentCharSequence(start + subStart, start + subEnd);
        }

        public boolean matchString(String s) {
            int len = s.length();

            if (len > length())
                return false;

            try {
                return s.equals(fDocument.get(start, len));
            }
            catch (BadLocationException e) {
                return false;
            }
        }

        public String toString() {
            return new StringBuilder(this).toString();
        }

        private class CharIterator implements Iterator {
            int index;
            public CharIterator() { index = 0; }
            public boolean hasNext() {
                return index < length();
            }
            public Object next() { return charAt(index++); }
            public void remove() { throw new UnsupportedOperationException(); }
        }

        public Iterator iterator() {
            return new CharIterator();
        }
    }

    public CharSequence readSequence() {
        return new DocumentCharSequence(fOffset, fRangeEnd);
    }

    // increments the offset by a given amount - this is from a match found
    public void moveForward(int offset){
        fOffset = fOffset + offset;
    }
}
