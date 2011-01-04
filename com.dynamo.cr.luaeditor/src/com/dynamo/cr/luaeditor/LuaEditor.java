package com.dynamo.cr.luaeditor;

import org.eclipse.ui.editors.text.TextEditor;

public class LuaEditor extends TextEditor {

	private ColorManager colorManager;

	public LuaEditor() {
		super();
		colorManager = new ColorManager();
		setSourceViewerConfiguration(new LuaConfiguration(colorManager));
		// TODO: Problem with save and crepo with this...
		//setDocumentProvider(new LuaDocumentProvider());
	}

	public void dispose() {
		colorManager.dispose();
		super.dispose();
	}

}
