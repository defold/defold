package com.defold.editor.eclipse;

import org.eclipse.fx.ui.controls.styledtext.StyledTextArea;
import org.eclipse.fx.ui.controls.styledtext.skin.StyledTextSkin;
import org.eclipse.fx.ui.controls.styledtext.behavior.StyledTextBehavior;
import javafx.scene.control.ListView;


/**
 * Styled text skin override so that we can add our on mouse event
 * handling on the content view
 */
@SuppressWarnings("restriction")
public class DefoldStyledTextSkin extends StyledTextSkin {
	private ListView<Line> defoldContentView;

       	public DefoldStyledTextSkin(StyledTextArea styledText) {
            super(styledText);
	}

	public DefoldStyledTextSkin(StyledTextArea styledText, StyledTextBehavior behavior) {
            super(styledText, behavior);
	}

	/**
	 * Set up the content viewer
	 *
	 * @param contentView
	 *            the content viewer
	 */
        @Override
	protected void initializeContentViewer(ListView<Line> contentView) {
            super.initializeContentViewer(contentView);
            defoldContentView = contentView;
	}

       public ListView<Line> getListView(){
           return defoldContentView;
       }

}
