package com.defold.editor.eclipse;


import org.eclipse.fx.ui.controls.styledtext.StyledTextArea;
import org.eclipse.fx.ui.controls.styledtext.behavior.StyledTextBehavior;

import javafx.scene.input.KeyEvent;

/**
 * Behavior for styled text
  Overrides for key event handlers until we can move to version 2.3.0
 */
public class DefoldStyledTextBehavior extends StyledTextBehavior{

	/**
	 * Create a new behavior
	 *
	 * @param styledText
	 *            the styled text control
	 */
	public DefoldStyledTextBehavior(StyledTextArea styledText) {
                super(styledText);
	}

	/**
	 * Handle key event
	 *
	 * @param arg0
	 *            the event
	 */
	protected void callActionForEvent(KeyEvent arg0) {
            // We are doing nothing here and handling all the events
            // in the code-view in the Defold editor instead
	}

}
