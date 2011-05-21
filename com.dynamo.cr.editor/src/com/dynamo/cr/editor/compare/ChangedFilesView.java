package com.dynamo.cr.editor.compare;

import java.lang.reflect.InvocationTargetException;
import java.util.Iterator;
import java.util.concurrent.Semaphore;

import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceChangeListener;
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

import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status;

public class ChangedFilesView extends ViewPart implements SelectionListener, IResourceChangeListener, ISelectionChangedListener, Listener {

    public final static String ID = "com.dynamo.crepo.changedfiles";

    private BranchStatusTableViewer tableViewer;
	private Menu menu;
    private MenuItem diffItem;
    private MenuItem revertItem;
    private UpdateThread updateThread;

    class UpdateThread extends Thread {

        private boolean quit = false;
        Semaphore semaphore = new Semaphore(0);

        @Override
        public void run() {

            while (!quit) {
                try {
                    this.semaphore.acquire();
                    this.semaphore.drainPermits();

                    IBranchClient client = Activator.getDefault().getBranchClient();
                    if (client != null) {
                        try {
                            BranchStatus status = client.getBranchStatus();

                            final java.util.List<ResourceStatus> resources = new java.util.ArrayList<ResourceStatus>(status.getFileStatusCount());
                            for (Status s : status.getFileStatusList()) {
                                resources.add(new ResourceStatus(s));
                            }

                            synchronized(this) {
                                Display.getDefault().asyncExec(new Runnable() {
                                    @Override
                                    public void run() {
                                        tableViewer.setInput(resources);
                                    }
                                });
                            }
                        } catch (RepositoryException e) {
                            Activator.getDefault().logger.warning(e.getMessage());
                        }
                    }
                    Thread.sleep(2000); // Do not update too often
                } catch (InterruptedException e) {
                }
            }
        }

        public void dispose() {
            this.quit  = true;
            this.interrupt();
            try {
                this.join();
            } catch (InterruptedException e) {
            }
        }

        public void update() {
            this.semaphore.release();
        }
    }

    public ChangedFilesView() {
        ResourcesPlugin.getWorkspace().addResourceChangeListener(this);
        this.updateThread = new UpdateThread();
        this.updateThread.start();
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
        this.updateThread.update();
    }

    @Override
    public void dispose() {
        super.dispose();
        ResourcesPlugin.getWorkspace().removeResourceChangeListener(this);
        this.updateThread.dispose();
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
                                e.printStackTrace();
                            }
                            subMonitor.done();
                        }
                        try {
                            ResourcesPlugin.getWorkspace().getRoot().refreshLocal(IResource.DEPTH_INFINITE, monitor);
                        } catch (CoreException e) {
                            e.printStackTrace();
                        }
                        monitor.done();
                    }
                }, null);
            } catch (Throwable e) {
                e.printStackTrace();
            }
        }
    }

    @Override
    public void resourceChanged(IResourceChangeEvent event) {
        this.updateThread.update();
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
}
