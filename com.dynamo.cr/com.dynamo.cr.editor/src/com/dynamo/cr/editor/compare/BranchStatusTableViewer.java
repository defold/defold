package com.dynamo.cr.editor.compare;

import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;

import org.eclipse.compare.CompareConfiguration;
import org.eclipse.compare.CompareEditorInput;
import org.eclipse.compare.CompareUI;
import org.eclipse.compare.structuremergeviewer.DiffNode;
import org.eclipse.compare.structuremergeviewer.Differencer;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.viewers.ColumnLabelProvider;
import org.eclipse.jface.viewers.DecoratingLabelProvider;
import org.eclipse.jface.viewers.DoubleClickEvent;
import org.eclipse.jface.viewers.IDoubleClickListener;
import org.eclipse.jface.viewers.ILabelProvider;
import org.eclipse.jface.viewers.IStructuredContentProvider;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.jface.viewers.TableViewer;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;

import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status;

public class BranchStatusTableViewer extends TableViewer {

    private boolean dialog;

    public BranchStatusTableViewer(Composite parent, boolean dialog) {
        super(parent);

        this.dialog = dialog;

        setContentProvider(new IStructuredContentProvider() {

            @Override
            public void inputChanged(Viewer viewer, Object oldInput, Object newInput) {
            }

            @Override
            public void dispose() {
            }

            @Override
            public Object[] getElements(Object inputElement) {
                @SuppressWarnings("unchecked")
                java.util.List<ResourceStatus> lst = (java.util.List<ResourceStatus>) inputElement;
                return lst.toArray(new ResourceStatus[lst.size()]);
            }
        });

        ILabelProvider labelProvider = new ColumnLabelProvider() {
            @Override
            public String getText(Object element) {
                ResourceStatus rs = (ResourceStatus) element;
                Status s = rs.getStatus();
                return s.getName().substring(1);
            }

            @Override
            public Image getImage(Object element) {
                if (element instanceof ResourceStatus) {
                    ResourceStatus resourceStatus = (ResourceStatus) element;
                    Status status = resourceStatus.getStatus();
                    if (status.getName().lastIndexOf('.') != -1) {
                        return Activator.getDefault().getImage(status.getName());
                    }
                }
                return super.getImage(element);
            }

            @Override
            public Color getForeground(Object element) {
                ResourceStatus rs = (ResourceStatus) element;
                Status s = rs.getStatus();
                if (s.getWorkingTreeStatus().equals(" ")) {
                    return Display.getCurrent().getSystemColor(SWT.COLOR_BLACK);
                } else {
                    return Display.getCurrent().getSystemColor(SWT.COLOR_DARK_RED);
                }
            }
        };

        setLabelProvider(new DecoratingLabelProvider(labelProvider, Activator.getDefault().getWorkbench().getDecoratorManager().getLabelDecorator()));
        setInput(new ArrayList<ResourceStatus>());

        addDoubleClickListener(new IDoubleClickListener() {

            @Override
            public void doubleClick(DoubleClickEvent event) {
                StructuredSelection selection = (StructuredSelection)event.getSelection();
                ResourceStatus resourceStatus = (ResourceStatus)selection.getFirstElement();
                openDiff(resourceStatus);
            }
        });

        getControl().addKeyListener(new KeyListener() {

            @Override
            public void keyReleased(KeyEvent e) {
                if (!getSelection().isEmpty() && e.keyCode == '\r') {
                    StructuredSelection selection = (StructuredSelection)getSelection();
                    ResourceStatus resourceStatus = (ResourceStatus)selection.getFirstElement();
                    openDiff(resourceStatus);
                }
            }

            @Override
            public void keyPressed(KeyEvent e) {
                // TODO Auto-generated method stub

            }
        });
    }

    public void openDiff(ResourceStatus resourceStatus) {
        final IBranchClient branchClient = Activator.getDefault().getBranchClient();
        final Status status = resourceStatus.getStatus();
        final String path = status.getName();
        final String originalPath = status.hasOriginal() ? status.getOriginal() : status.getName();
        CompareConfiguration config = new CompareConfiguration();
        config.setLeftEditable(false);
        config.setLeftLabel(String.format("%s#master", originalPath));
        config.setRightEditable(true);
        config.setRightLabel(String.format("%s#yours", path));
        CompareEditorInput input = new CompareEditorInput(config) {

            @Override
            protected Object prepareInput(IProgressMonitor monitor)
                    throws InvocationTargetException, InterruptedException {
                CompareItem left = new CompareItem(branchClient, originalPath, "master");
                CompareItem right = null;
                if (!status.getIndexStatus().equals("D")) {
                    right = new CompareItem(branchClient, path, "");
                }
                return new DiffNode(null, Differencer.CHANGE, null, left, right);
            }

        };
        if (this.dialog) {
            CompareUI.openCompareDialog(input);
        } else {
            CompareUI.openCompareEditor(input);
        }
    }
}
