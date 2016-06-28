package com.defold.editor.eclipse;

import java.util.List;

import org.eclipse.fx.ui.controls.styledtext.StyledTextArea;
import org.eclipse.fx.ui.controls.styledtext.StyledTextLayoutContainer;
import org.eclipse.fx.ui.controls.styledtext.behavior.StyledTextBehavior;
import com.defold.editor.eclipse.DefoldStyledTextSkin.LineCell;

import javafx.event.Event;
import javafx.geometry.Bounds;
import javafx.scene.input.MouseEvent;

public class DefoldStyledTextBehavior extends StyledTextBehavior {
	

	
	public DefoldStyledTextBehavior(StyledTextArea styledText) {
		
		super(styledText);

	}
	
	public void defoldUpdateCursor(MouseEvent event, List<LineCell> visibleCells, boolean selection) {
		//proxied in the Clojure code-view
	}

}
