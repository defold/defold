package com.dynamo.cr.editor.handlers;

import java.lang.reflect.InvocationTargetException;

import org.eclipse.compare.CompareConfiguration;
import org.eclipse.compare.CompareEditorInput;
import org.eclipse.compare.CompareUI;
import org.eclipse.compare.structuremergeviewer.DiffNode;
import org.eclipse.compare.structuremergeviewer.Differencer;
import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Table;
import org.eclipse.swt.widgets.Widget;

import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.compare.CompareItem;
import com.dynamo.cr.editor.compare.ConflictedResourceStatus;

public class DiffConflictHandler extends AbstractHandler {

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
            final String path = conflict.getStatus().getName();
            final IBranchClient branchClient = Activator.getDefault().getBranchClient();
            CompareConfiguration config = new CompareConfiguration();
            config.setAncestorLabel(String.format("%s#base", path));
            config.setLeftEditable(false);
            config.setLeftLabel(String.format("%s#theirs", path));
            config.setLeftImage(Activator.getDefault().getImageRegistry().get(Activator.THEIRS_IMAGE_ID));
            config.setRightEditable(false);
            config.setRightLabel(String.format("%s#yours", path));
            config.setRightImage(Activator.getDefault().getImageRegistry().get(Activator.YOURS_IMAGE_ID));
            CompareEditorInput input = new CompareEditorInput(config) {

                @Override
                protected Object prepareInput(IProgressMonitor monitor)
                        throws InvocationTargetException, InterruptedException {
                    CompareItem baseItem = new CompareItem(branchClient, path, ":1");
                    CompareItem yoursItem = new CompareItem(branchClient, path, ":2");
                    CompareItem theirsItem = new CompareItem(branchClient, path, ":3");
                    return new DiffNode(null, Differencer.CONFLICTING, baseItem, theirsItem, yoursItem);
                }

            };
            CompareUI.openCompareDialog(input);
        }
        return null;
    }

}
