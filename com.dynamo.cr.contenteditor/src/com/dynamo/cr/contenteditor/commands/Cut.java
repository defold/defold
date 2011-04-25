package com.dynamo.cr.contenteditor.commands;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;

public class Cut extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        Copy copy = new Copy();
        copy.execute(event);

        Delete delete = new Delete();
        delete.execute(event);

        return null;
    }
}
