package com.dynamo.cr.cgeditor;

import org.eclipse.jface.text.rules.IWhitespaceDetector;

public class CgWhitespaceDetector implements IWhitespaceDetector {

	public boolean isWhitespace(char c) {
		return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
	}
}
