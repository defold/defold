package com.dynamo.cr.cgeditor;

import org.eclipse.ui.editors.text.TextEditor;

public class CgEditor extends TextEditor {

	private ColorManager colorManager;

	public CgEditor() {
		super();
		colorManager = new ColorManager();
		setSourceViewerConfiguration(new CgConfiguration(colorManager));
		// TODO: Problem with save and crepo with this...
		//setDocumentProvider(new CgDocumentProvider());
	}

	public void dispose() {
		colorManager.dispose();
		super.dispose();
	}

}
