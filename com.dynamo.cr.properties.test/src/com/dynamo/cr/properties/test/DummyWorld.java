package com.dynamo.cr.properties.test;

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IPath;

import com.dynamo.cr.properties.IPropertyObjectWorld;

class DummyWorld implements IPropertyObjectWorld {
    Map<String, Integer> commandsCreated = new HashMap<String, Integer>();
    int totalCommands = 0;
    IContainer contentRoot;

    public DummyWorld() {
        IFile file = mock(IFile.class);
        when(file.exists()).thenReturn(false);
        this.contentRoot = mock(IContainer.class);
        when(this.contentRoot.getFile(any(IPath.class))).thenReturn(file);
    }

    @Override
    public IContainer getContentRoot() {
        return this.contentRoot;
    }
}