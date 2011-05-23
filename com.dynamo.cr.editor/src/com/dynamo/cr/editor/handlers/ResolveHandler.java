package com.dynamo.cr.editor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Table;
import org.eclipse.swt.widgets.Widget;

import com.dynamo.cr.editor.compare.ConflictedResourceStatus;
import com.dynamo.cr.editor.compare.ConflictedResourceStatus.Resolve;

public class ResolveHandler extends AbstractHandler {

    private final static String PARAMETER_ID = "com.dynamo.cr.editor.commands.resolve.type";

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        Table table = null;
        Widget widget = ((Event)event.getTrigger()).widget;
        if (widget instanceof Table) {
            table = (Table)widget;
        } else if (widget.getData() instanceof Table) {
            table = (Table)widget.getData();
        }
        if (table != null && table.getSelectionCount() > 0) {
            ConflictedResourceStatus conflict = (ConflictedResourceStatus)table.getSelection()[0].getData();
            String value = event.getParameter(PARAMETER_ID);
            if (value.equals("yours")) {
                conflict.setResolve(Resolve.YOURS);
            } else if (value.equals("theirs")) {
                conflict.setResolve(Resolve.THEIRS);
            }
        }
        return null;
    }

}
