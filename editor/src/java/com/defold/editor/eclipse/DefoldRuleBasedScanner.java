package com.defold.editor.eclipse;

import org.eclipse.jface.text.rules.RuleBasedScanner;
import org.eclipse.jface.text.BadLocationException;

public class DefoldRuleBasedScanner extends RuleBasedScanner {

    //Reads from offset all at once to the end of the doc as a peek
    public String readString() {
        try {
            int docLen = fDocument.getLength();
            if (fOffset < docLen){
                 int len = docLen - fOffset;
                 return fDocument.get(fOffset, len);
            }else{
                return "";
            }
        } catch (BadLocationException e) {
            System.out.println("Bad Location in Document" + "offset:" + fOffset +  "length:" + fDocument.getLength());
            return "";
        }
    }

    // increments the offset by a given amount - this is from a match found
    public void moveForward(int offset){
        fOffset = fOffset + offset;
    }
}
