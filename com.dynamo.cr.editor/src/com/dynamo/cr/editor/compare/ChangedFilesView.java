package com.dynamo.cr.editor.compare;

import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.concurrent.Semaphore;

import org.eclipse.compare.CompareConfiguration;
import org.eclipse.compare.CompareEditorInput;
import org.eclipse.compare.CompareUI;
import org.eclipse.compare.structuremergeviewer.DiffNode;
import org.eclipse.compare.structuremergeviewer.Differencer;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceChangeListener;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.Platform;
import org.eclipse.core.runtime.QualifiedName;
import org.eclipse.core.runtime.SubProgressMonitor;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.jface.viewers.ColumnLabelProvider;
import org.eclipse.jface.viewers.DecoratingLabelProvider;
import org.eclipse.jface.viewers.ILabelProvider;
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
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status;

public class ChangedFilesView extends ViewPart implements SelectionListener, IResourceChangeListener, ISelectionChangedListener, Listener {

    public final static String ID = "com.dynamo.crepo.changedfiles";
    private final static QualifiedName GIT_INDEX_STATUS_PROPERTY = new QualifiedName(Activator.PLUGIN_ID, "gitIndexStatus");
    private TableViewer tableViewer;
	private Menu menu;
    private MenuItem diffItem;
    private MenuItem revertItem;
    private UpdateThread updateThread;

    class ResourceStatus implements IAdaptable {
        private Status status;
        private IResource resource;

        public ResourceStatus(Status status) {
            this.status = status;
            this.resource = EditorUtil.getContentRoot(Activator.getDefault().getProject()).findMember(status.getName());
        }

        public Status getStatus() {
            return status;
        }

        public IResource getResource() {
            return resource;
        }

        @Override
        public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
            return Platform.getAdapterManager().getAdapter(this, adapter);
        }
    }

	class UpdateThread extends Thread {

	    private boolean quit = false;
        Semaphore semaphore = new Semaphore(0);

        private synchronized void updateUI(java.util.List<ResourceStatus> files) {
            @SuppressWarnings("unchecked")
            java.util.List<ResourceStatus> currentItems = (java.util.List<ResourceStatus>) tableViewer.getInput();
            boolean changed = false;
            if (files.size() == currentItems.size()) {
                for (int i = 0; i < currentItems.size(); i++) {
                    Status currentStatus = currentItems.get(i).getStatus();
                    Status status = files.get(i).getStatus();
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

            try {
                if (changed) {
                    for (ResourceStatus s : currentItems) {
                        IResource resource = s.getResource();
                        if (resource != null) {
                            resource.setSessionProperty(GIT_INDEX_STATUS_PROPERTY, null);
                        }
                    }
                    for (ResourceStatus s : files) {
                        IResource resource = s.getResource();
                        if (resource != null) {
                            resource.setSessionProperty(GIT_INDEX_STATUS_PROPERTY, s.getStatus().getIndexStatus());
                        }
                    }
                    tableViewer.setInput(files);
                }
            } catch (CoreException e) {
                e.printStackTrace();
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

	                        final java.util.List<ResourceStatus> resources = new java.util.ArrayList<ResourceStatus>(status.getFileStatusCount());
	                        for (Status s : status.getFileStatusList()) {
	                            resources.add(new ResourceStatus(s));
	                        }

	                        synchronized(this) {
    	                        Display.getDefault().asyncExec(new Runnable() {
                                    @Override
                                    public void run() {
                                        updateUI(resources);
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
                java.util.List<ResourceStatus> lst = (java.util.List<ResourceStatus>) inputElement;
                return lst.toArray(new ResourceStatus[lst.size()]);
            }
        });

        ILabelProvider labelProvider = new ColumnLabelProvider() {
            @Override
            public String getText(Object element) {
                ResourceStatus rs = (ResourceStatus) element;
                Status s = rs.getStatus();
                if (s.getWorkingTreeStatus().equals(" ")) {
                    return String.format("[%s] %s", s.getIndexStatus(), s.getName().substring(1));
                } else {
                    return String.format("[%s%s] %s", s.getIndexStatus(), s.getWorkingTreeStatus(), s.getName().substring(1));
                }
            }

            @Override
            public Image getImage(Object element) {
                if (element instanceof ResourceStatus) {
                    ResourceStatus resourceStatus = (ResourceStatus) element;
                    Status status = resourceStatus.getStatus();
                    if (status.getName().lastIndexOf('.') != -1) {
                        return PlatformUI.getWorkbench().getEditorRegistry().getImageDescriptor(status.getName().substring(status.getName().lastIndexOf('.'))).createImage();
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
        tableViewer.setLabelProvider(new DecoratingLabelProvider(labelProvider, Activator.getDefault().getWorkbench().getDecoratorManager().getLabelDecorator()));
        tableViewer.setInput(new ArrayList<ResourceStatus>());

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
        final
        Iterator<ResourceStatus> iterator = structSelection.iterator();

        if (event.widget == this.diffItem) {
            final ResourceStatus resourceStatus = iterator.next();
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
                    CompareItem left = new CompareItem(branch_client, originalPath, "master");
                    CompareItem right = null;
                    if (!status.getIndexStatus().equals("D")) {
                        right = new CompareItem(branch_client, path, "");
                    }
                    return new DiffNode(null, Differencer.CHANGE, null, left, right);
                }

            };
            CompareUI.openCompareEditor(input);
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
