package com.dynamo.cr.luaeditor;

import org.eclipse.jface.text.rules.IWhitespaceDetector;

public class LuaWhitespaceDetector implements IWhitespaceDetector {

	public boolean isWhitespace(char c) {
		return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
	}
}
