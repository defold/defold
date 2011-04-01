package com.dynamo.cr.editor.compare;

import java.lang.reflect.InvocationTargetException;
import java.net.URI;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.concurrent.Semaphore;

import javax.ws.rs.core.UriBuilder;

import org.eclipse.compare.CompareConfiguration;
import org.eclipse.compare.CompareEditorInput;
import org.eclipse.compare.CompareUI;
import org.eclipse.compare.structuremergeviewer.DiffNode;
import org.eclipse.compare.structuremergeviewer.Differencer;
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
import org.eclipse.jface.viewers.ColumnLabelProvider;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.IStructuredContentProvider;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.TableViewer;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Image;
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
    private TableViewer tableViewer;
	private Menu menu;
    private MenuItem diffItem;
    private MenuItem revertItem;
    private UpdateThread updateThread;

	class UpdateThread extends Thread {

	    private boolean quit = false;
        Semaphore semaphore = new Semaphore(0);

        private synchronized void updateUI(java.util.List<Status> files) {
            @SuppressWarnings("unchecked")
            java.util.List<Status> currentItems = (java.util.List<Status>) tableViewer.getInput();
            boolean changed = false;
            if (files.size() == currentItems.size()) {
                for (int i = 0; i < currentItems.size(); i++) {
                    Status currentStatus = currentItems.get(i);
                    Status status = files.get(i);
                    if (!currentStatus.getName().equals(status.getName())
                            || !currentStatus.getIndexStatus().equals(status.getIndexStatus())
                            || !currentStatus.getWorkingTreeStatus().equals(status.getWorkingTreeStatus())) {
                        changed = true;
                        break;
                    }
                }
            } else {
                changed = true;
            }

            if (changed) {
                tableViewer.setInput(files);
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
        tableViewer = new TableViewer(parent);
        tableViewer.addSelectionChangedListener(this);
        tableViewer.setContentProvider(new IStructuredContentProvider() {

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

        tableViewer.setLabelProvider(new ColumnLabelProvider() {
            @Override
            public String getText(Object element) {
                Status s = (Status) element;
                if (s.getWorkingTreeStatus().equals(" ")) {
                    return String.format("[%s] %s", s.getIndexStatus(), s.getName().substring(1));
                } else {
                    return String.format("[%s%s] %s", s.getIndexStatus(), s.getWorkingTreeStatus(), s.getName().substring(1));
                }
            }

            @Override
            public Image getImage(Object element) {
                if (element instanceof Status) {
                    Status status = (Status) element;
                    if (status.getName().lastIndexOf('.') != -1) {
                        return PlatformUI.getWorkbench().getEditorRegistry().getImageDescriptor(status.getName().substring(status.getName().lastIndexOf('.'))).createImage();
                    }
                }
                return super.getImage(element);
            }

            @Override
            public Color getForeground(Object element) {
                Status s = (Status) element;
                if (s.getWorkingTreeStatus().equals(" ")) {
                    return Display.getCurrent().getSystemColor(SWT.COLOR_BLACK);
                } else {
                    return Display.getCurrent().getSystemColor(SWT.COLOR_DARK_RED);
                }
            }
        });
        tableViewer.setInput(new ArrayList<Status>());

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
        ISelection selection = tableViewer.getSelection();
        if (selection.isEmpty())
            return;

        final IStructuredSelection structSelection = (IStructuredSelection) selection;
        @SuppressWarnings("unchecked")
        final
        Iterator<Status> iterator = structSelection.iterator();

        if (event.widget == this.diffItem) {
            Status status = iterator.next();
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
                    CompareItem left = new CompareItem(branch_client, originalPath, "master");
                    CompareItem right = new CompareItem(branch_client, path, "");
                    return new DiffNode(null, Differencer.CHANGE, null, left, right);
                }

            };
            CompareUI.openCompareDialog(input);
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
