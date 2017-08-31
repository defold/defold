package com.defold.editor.eclipse;

import java.io.OutputStream;
import java.io.PrintStream;

import org.eclipse.fx.text.ui.source.SourceViewer;
import org.eclipse.fx.ui.controls.styledtext.StyledTextArea;
import org.eclipse.jface.text.IDocument;

public class DefoldSourceViewer extends SourceViewer {

	@Override
	protected StyledTextArea createTextWidget() {
		StyledTextArea styledText= new DefoldStyledTextArea();
		styledText.setLineRulerVisible(true);
//		styledText.setLeftMargin(Math.max(styledText.getLeftMargin(), 2));
		return styledText;
	}

    public void setDocument(IDocument document) {
        // Behold, a terrible hack!
        // Temporarily disable System.err because e(fx)clipse logs the document adapter to System.err each time this method is called
        PrintStream err = System.err;
        try {
            System.setErr(new PrintStream(new OutputStream() {
                public void write(int b) {
                }
            }));
            super.setDocument(document);
        } finally {
            System.setErr(err);
        }
    }
}
