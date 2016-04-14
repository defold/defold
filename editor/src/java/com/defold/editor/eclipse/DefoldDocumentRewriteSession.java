package com.defold.editor.eclipse;

import org.eclipse.jface.text.DocumentRewriteSessionType;
import org.eclipse.jface.text.DocumentRewriteSession;

public class DefoldDocumentRewriteSession extends org.eclipse.jface.text.DocumentRewriteSession {

	public DefoldDocumentRewriteSession(DocumentRewriteSessionType sessionType) {
            super(sessionType);
	}
}
