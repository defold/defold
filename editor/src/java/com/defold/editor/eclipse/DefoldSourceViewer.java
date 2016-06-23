package com.defold.editor.eclipse;

import org.eclipse.fx.text.ui.source.SourceViewer;
import org.eclipse.fx.ui.controls.styledtext.StyledTextArea;

public class DefoldSourceViewer extends SourceViewer {
	
	
	@Override
	protected StyledTextArea createTextWidget() {
		System.err.println ("Carin creating widget!");
		StyledTextArea styledText= new DefoldStyledTextArea();
		styledText.setLineRulerVisible(true);
//		styledText.setLeftMargin(Math.max(styledText.getLeftMargin(), 2));
		return styledText;
	}

}
