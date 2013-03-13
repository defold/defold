package com.dynamo.cr.editor.compare;

import java.lang.reflect.InvocationTargetException;
import java.util.Iterator;

import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.SubProgressMonitor;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.swt.widgets.MenuItem;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.part.ViewPart;
import org.eclipse.ui.progress.IProgressService;

import com.dynamo.cr.client.BranchStatusChangedEvent;
import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.IBranchListener;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.services.IBranchService;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status;

public class ChangedFilesView extends ViewPart implements SelectionListener, IBranchListener, ISelectionChangedListener, Listener {

    public final static String ID = "com.dynamo.crepo.changedfiles";

    private BranchStatusTableViewer tableViewer;
	private Menu menu;
    private MenuItem diffItem;
    private MenuItem revertItem;

    public ChangedFilesView() {
        IBranchService branchService = (IBranchService)PlatformUI.getWorkbench().getService(IBranchService.class);
        if (branchService != null) {
            branchService.addBranchListener(this);
        }
    }

    @Override
    public void createPartControl(Composite parent) {
        tableViewer = new BranchStatusTableViewer(parent, false);
        tableViewer.addSelectionChangedListener(this);

        menu = new Menu(parent.getShell(), SWT.POP_UP);
        diffItem = new MenuItem(menu, SWT.PUSH);
        diffItem.setText("Diff");
        diffItem.setEnabled(false);
        diffItem.addListener(SWT.Selection, this);
        revertItem = new MenuItem(menu, SWT.PUSH);
        revertItem.setText("Revert");
        revertItem.setEnabled(false);
        revertItem.addListener(SWT.Selection, this);
        tableViewer.getTable().setMenu(menu);

        IBranchClient branchClient = Activator.getDefault().getBranchClient();
        if (branchClient != null) {
            try {
                BranchStatus branchStatus = branchClient.getBranchStatus(false);
                updateTable(branchStatus);
            } catch (RepositoryException e) {
                Activator.logException(e);
            }
        }
    }

    @Override
    public void dispose() {
        super.dispose();
        IBranchService branchService = (IBranchService)PlatformUI.getWorkbench().getService(IBranchService.class);
        if (branchService != null) {
            branchService.removeBranchListener(this);
        }
    }

    @Override
    public void setFocus() {
    }

	@Override
	public void widgetSelected(SelectionEvent e) {
	}

	@Override
	public void widgetDefaultSelected(SelectionEvent e) {
	}

	@Override
	public void handleEvent(Event event) {
        final IBranchClient branch_client = Activator.getDefault().getBranchClient();
        ISelection selection = tableViewer.getSelection();
        if (selection.isEmpty())
            return;

        final IStructuredSelection structSelection = (IStructuredSelection) selection;
        @SuppressWarnings("unchecked")
        final Iterator<ResourceStatus> iterator = structSelection.iterator();

        if (event.widget == this.diffItem) {
            ResourceStatus resourceStatus = iterator.next();
            tableViewer.openDiff(resourceStatus);
        } else {

            if (iterator.hasNext()) {
                IProgressService service = PlatformUI.getWorkbench().getProgressService();
                try {
                    service.runInUI(service, new IRunnableWithProgress() {

                    @Override
                    public void run(IProgressMonitor monitor)
                            throws InvocationTargetException,
                            InterruptedException {
                        monitor.beginTask("Reverting", structSelection.size());

                        while (iterator.hasNext()) {
                            final ResourceStatus resourceStatus = iterator.next();
                            final Status status = resourceStatus.getStatus();
                                SubProgressMonitor subMonitor = new SubProgressMonitor(monitor, 1);
                                subMonitor.subTask(status.getName());
                                subMonitor.beginTask(status.getName(), 1);
                                String path = status.getName();

                                try {
                                    branch_client.revertResource(path);
                                    subMonitor.worked(1);
                                }
                                catch (Exception e) {
                                    Activator.logException(e);
                                }
                                subMonitor.done();
                            }
                            try {
                                ResourcesPlugin.getWorkspace().getRoot().refreshLocal(IResource.DEPTH_INFINITE, monitor);
                            } catch (CoreException e) {
                                Activator.logException(e);
                            }
                            monitor.done();
                        }
                        {
                            IBranchService branchService = (IBranchService)PlatformUI.getWorkbench().getService(IBranchService.class);
                            if (branchService != null) {
                                branchService.updateBranchStatus(null);
                            }
                        }
                    }, null);
                } catch (Throwable e) {
                    Activator.logException(e);
                }
            }
        }
    }

    @Override
    public void selectionChanged(SelectionChangedEvent event) {
        boolean diffEnabled = false;
        if (!tableViewer.getSelection().isEmpty()) {
            IStructuredSelection selection = (IStructuredSelection)tableViewer.getSelection();
            diffEnabled = selection.size() == 1;
        }
        if (diffItem.isEnabled() != diffEnabled)
            diffItem.setEnabled(diffEnabled);
        boolean revertEnabled = !tableViewer.getSelection().isEmpty();
        if (revertItem.isEnabled() != revertEnabled)
            revertItem.setEnabled(revertEnabled);
    }

    @Override
    public void branchStatusChanged(BranchStatusChangedEvent event) {
        updateTable(event.getBranchStatus());
    }

    private void updateTable(BranchStatus branchStatus) {
        final java.util.List<ResourceStatus> resources = new java.util.ArrayList<ResourceStatus>(branchStatus.getFileStatusCount());
        for (Status s : branchStatus.getFileStatusList()) {
            resources.add(new ResourceStatus(s));
        }

        synchronized(this) {
            Display.getDefault().asyncExec(new Runnable() {
                @Override
                public void run() {
                    // The underlying widget could be disposed when we get here (e.g. program termination)
                    if (!tableViewer.getTable().isDisposed()) {
                        tableViewer.setInput(resources);
                    }
                }
            });
        }
    }
}
