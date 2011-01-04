package com.dynamo.cr.editor.status;

import java.lang.reflect.InvocationTargetException;
import java.net.URI;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.concurrent.Semaphore;

import javax.ws.rs.core.UriBuilder;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceChangeListener;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.SubProgressMonitor;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.IStructuredContentProvider;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.ListViewer;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.Viewer;
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
import com.dynamo.cr.editor.IRepositoryListener;
import com.dynamo.cr.editor.RepositoryChangeEvent;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status;

public class ChangedFilesView extends ViewPart implements SelectionListener, IRepositoryListener, IResourceChangeListener, ISelectionChangedListener, Listener {

    public final static String ID = "com.dynamo.crepo.changedfiles";
    private ListViewer listViewer;
	private Menu menu;
	private MenuItem revertItem;
    private UpdateThread updateThread;

	class UpdateThread extends Thread {

	    private boolean quit = false;
        Semaphore semaphore = new Semaphore(0);

        private synchronized void updateUI(java.util.List<Status> files) {
            @SuppressWarnings("unchecked")
            java.util.List<Status> currentItems = (java.util.List<Status>) listViewer.getInput();
            boolean changed = false;
            if (files.size() == currentItems.size()) {
                for (int i = 0; i < currentItems.size(); i++) {
                    if (!currentItems.get(i).getName().equals(files.get(i).getName())) {
                        changed = true;
                        break;
                    }
                }
            }
            else {
                changed = true;
            }

            if (changed) {
                listViewer.setInput(files);
            }
        }

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
	                        final java.util.List<Status> files = status.getFileStatusList();

	                        synchronized(this) {
    	                        Display.getDefault().asyncExec(new Runnable() {
                                    @Override
                                    public void run() {
                                        updateUI(files);
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
        Activator.getDefault().addRepositoryListener(this);
        ResourcesPlugin.getWorkspace().addResourceChangeListener(this);
        this.updateThread = new UpdateThread();
        this.updateThread.start();
    }

    @Override
    public void createPartControl(Composite parent) {
        listViewer = new ListViewer(parent);
        listViewer.addSelectionChangedListener(this);
        listViewer.setContentProvider(new IStructuredContentProvider() {

            @Override
            public void inputChanged(Viewer viewer, Object oldInput, Object newInput) {
            }

            @Override
            public void dispose() {
            }

            @Override
            public Object[] getElements(Object inputElement) {
                @SuppressWarnings("unchecked")
                java.util.List<Status> lst = (java.util.List<Status>) inputElement;
                return lst.toArray(new Status[lst.size()]);
            }
        });

        listViewer.setLabelProvider(new LabelProvider() {
           @Override
           public String getText(Object element) {
               Status s = (Status) element;
               return String.format("[%s] %s", s.getStatus(), s.getName().substring(1));
           }
        });
        listViewer.setInput(new ArrayList<Status>());

        menu = new Menu(parent.getShell(), SWT.POP_UP);
        revertItem = new MenuItem(menu, SWT.PUSH);
        revertItem.setText("Revert");
        revertItem.setEnabled(false);
        revertItem.addListener(SWT.Selection, this);
        listViewer.getList().setMenu(menu);
    }

    @Override
    public void dispose() {
        super.dispose();
        ResourcesPlugin.getWorkspace().removeResourceChangeListener(this);
        Activator.getDefault().removeRepositoryListener(this);
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

	IResource findResourceFromURI(URI uri, String path) {
	    IResource resource = null;
	    for (IContainer c : ResourcesPlugin.getWorkspace().getRoot().findContainersForLocationURI(uri)) {
	        resource = c.findMember(path);
	        if (resource != null && resource.exists())
	            return resource;

	    }
	    return null;
	}

	@Override
	public void handleEvent(Event event) {
        final IBranchClient branch_client = Activator.getDefault().getBranchClient();
        ISelection selection = listViewer.getSelection();
        if (selection.isEmpty())
            return;

        final IStructuredSelection structSelection = (IStructuredSelection) selection;
        @SuppressWarnings("unchecked")
        final
        Iterator<Status> iterator = structSelection.iterator();

        IProgressService service = PlatformUI.getWorkbench().getProgressService();
        try {
            service.runInUI(service, new IRunnableWithProgress() {

            @Override
            public void run(IProgressMonitor monitor)
                    throws InvocationTargetException,
                    InterruptedException {
                monitor.beginTask("Reverting", structSelection.size());

                while (iterator.hasNext()) {
                    final Status status = iterator.next();
                        SubProgressMonitor subMonitor = new SubProgressMonitor(monitor, 1);
                        subMonitor.subTask(status.getName());
                        subMonitor.beginTask(status.getName(), 3);
                        String path = status.getName();
                        URI uri = UriBuilder.fromUri(branch_client.getURI()).scheme("crepo").queryParam("path", path).build();

                        IResource revert_resource = findResourceFromURI(uri, path);

                        try {
                            branch_client.revertResource(path);
                            subMonitor.worked(1);
                            Activator.getDefault().sendBranchChanged();

                            if (revert_resource != null) {
                                revert_resource.getParent().refreshLocal(IResource.DEPTH_INFINITE, null);
                                subMonitor.worked(1);
                            }

                            if (status.hasOriginal()) {
                                // We can't use getParent(). The does not "exists" - yet.
                                IPath originalPath = new Path(status.getOriginal());
                                IPath refreshPath = originalPath.removeLastSegments(1);
                                URI originalUri = UriBuilder.fromUri(branch_client.getURI()).scheme("crepo").queryParam("path", refreshPath.toPortableString()).build();
                                IResource originalResource = findResourceFromURI(originalUri, refreshPath.toPortableString());
                                if (originalResource != null) {
                                    originalResource.refreshLocal(IResource.DEPTH_INFINITE, null);
                                    subMonitor.worked(1);
                                }
                            }
                        }
                        catch (Exception e) {

                        }
                        subMonitor.done();
                    }
                    monitor.done();
                }
            }, null);
        } catch (Throwable e) {
            e.printStackTrace();
        }
    }

    @Override
    public void branchChanged(RepositoryChangeEvent e) {
        this.updateThread.update();
    }

    @Override
    public void resourceChanged(IResourceChangeEvent event) {
        this.updateThread.update();
    }

    @Override
    public void selectionChanged(SelectionChangedEvent event) {
        boolean enable = !listViewer.getSelection().isEmpty();
        if (revertItem.isEnabled() != enable)
            revertItem.setEnabled(enable);
    }
}
