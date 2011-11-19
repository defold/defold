package com.dynamo.cr.luaeditor;

import org.eclipse.ui.editors.text.TextEditor;

public class LuaEditor extends TextEditor {

	private ColorManager colorManager;

	public LuaEditor() {
		super();
		colorManager = new ColorManager();
		setSourceViewerConfiguration(new LuaConfiguration(colorManager));
	}

	public void dispose() {
		colorManager.dispose();
		super.dispose();
	}

}
