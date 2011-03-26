package com.dynamo.cr.editor.core;

import org.eclipse.core.resources.IContainer;

public class ResourceRefactorContext {

    private IContainer contentRoot;

    public ResourceRefactorContext(IContainer contentRoot) {
        this.contentRoot = contentRoot;
    }

    public IContainer getContentRoot() {
        return contentRoot;
    }

}
