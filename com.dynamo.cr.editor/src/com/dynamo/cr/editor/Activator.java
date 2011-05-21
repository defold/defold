package com.dynamo.cr.editor;

import java.lang.reflect.InvocationTargetException;
import java.net.URI;
import java.util.concurrent.Semaphore;
import java.util.logging.Logger;

import javax.ws.rs.core.UriBuilder;

import org.eclipse.core.net.proxy.IProxyData;
import org.eclipse.core.net.proxy.IProxyService;
import org.eclipse.core.resources.ICommand;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IProjectDescription;
import org.eclipse.core.resources.IProjectNature;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceChangeListener;
import org.eclipse.core.resources.IWorkspace;
import org.eclipse.core.resources.IWorkspaceDescription;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.dialogs.ErrorDialog;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.jface.util.IPropertyChangeListener;
import org.eclipse.jface.util.PropertyChangeEvent;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.navigator.resources.ProjectExplorer;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.eclipse.ui.progress.IProgressService;
import org.osgi.framework.BundleContext;
import org.osgi.util.tracker.ServiceTracker;

import com.dynamo.cr.client.ClientFactory;
import com.dynamo.cr.client.ClientUtils;
import com.dynamo.cr.client.DelegatingClientFactory;
import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.IClientFactory;
import com.dynamo.cr.client.IProjectClient;
import com.dynamo.cr.client.IProjectsClient;
import com.dynamo.cr.client.IUsersClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.editor.compare.ResourceStatus;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.decorators.BranchStatusDecorator;
import com.dynamo.cr.editor.dialogs.DialogUtil;
import com.dynamo.cr.editor.fs.RepositoryFileSystemPlugin;
import com.dynamo.cr.editor.preferences.PreferenceConstants;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;
import com.sun.jersey.api.client.filter.HTTPBasicAuthFilter;

public class Activator extends AbstractUIPlugin implements IPropertyChangeListener, IResourceChangeListener {

    // The plug-in ID
    public static final String PLUGIN_ID = "com.dynamo.cr.editor"; //$NON-NLS-1$

    // Shared images
    public static final String OVERLAY_ERROR_IMAGE_ID = "OVERLAY_ERROR";
    public static final String OVERLAY_EDIT_IMAGE_ID = "OVERLAY_EDIT";
    public static final String OVERLAY_ADD_IMAGE_ID = "OVERLAY_ADD";
    public static final String OVERLAY_DELETE_IMAGE_ID = "OVERLAY_DELETE";
    public static final String UPDATE_LARGE_IMAGE_ID = "UPDATE_LARGE";
    public static final String COMMIT_LARGE_IMAGE_ID = "COMMIT_LARGE";
    public static final String RESOLVE_LARGE_IMAGE_ID = "RESOLVE_LARGE";
    public static final String DONE_LARGE_IMAGE_ID = "DONE_LARGE";
    public static final String ERROR_LARGE_IMAGE_ID = "ERROR_LARGE";
    public static final String UNRESOLVED_IMAGE_ID = "UNRESOLVED";
    public static final String YOURS_IMAGE_ID = "YOURS";
    public static final String THEIRS_IMAGE_ID = "THEIRS";

    // The shared instance
    private static Activator plugin;

	private static BundleContext context;

	static BundleContext getContext() {
		return context;
	}

    /**
     * Returns the shared instance
     *
     * @return the shared instance
     */
    public static Activator getDefault() {
        return plugin;
    }

    /**
     * Returns an image descriptor for the image file at the given
     * plug-in relative path
     *
     * @param path the path
     * @return the image descriptor
     */
    public static ImageDescriptor getImageDescriptor(String path) {
        return imageDescriptorFromPlugin(PLUGIN_ID, path);
    }

    public IProjectClient projectClient;

    private IBranchClient branchClient;

    public String activeBranch;

    public Logger logger;

    private IClientFactory factory;

    private ServiceTracker proxyTracker;

    public UserInfo userInfo;

    public IProjectsClient projectsClient;

    private IProject project;

    private BranchStatusUpdateThread branchStatusUpdateThread;

    public IProxyService getProxyService() {
        return (IProxyService) proxyTracker.getService();
    }

    public IProject getProject() {
        return project;
    }

    void deleteAllCrProjects() throws CoreException {
        IProject[] projects = ResourcesPlugin.getWorkspace().getRoot().getProjects();
        for (IProject p : projects) {
            if (p.isOpen()) {
                IProjectNature nature = p.getNature("com.dynamo.cr.editor.core.crnature");
                if (nature != null) {
                    p.delete(true, new NullProgressMonitor());
                }
            }
        }
    }

	/*
	 * (non-Javadoc)
	 * @see org.osgi.framework.BundleActivator#start(org.osgi.framework.BundleContext)
	 */
	public void start(BundleContext bundleContext) throws Exception {
	    super.start(bundleContext);
	    this.logger = Logger.getLogger(Activator.PLUGIN_ID);

        proxyTracker = new ServiceTracker(bundleContext, IProxyService.class
                .getName(), null);
        proxyTracker.open();

        plugin = this;
		Activator.context = bundleContext;
		connectProjectClient();
        IPreferenceStore store = getPreferenceStore();
        store.addPropertyChangeListener(this);
        updateSocksProxy();
        deleteAllCrProjects();

        // Disable auto-building of projects
        IWorkspace workspace = ResourcesPlugin.getWorkspace();
        IWorkspaceDescription work_desc = ResourcesPlugin.getWorkspace().getDescription();
        work_desc.setAutoBuilding(false);
        try {
            workspace.setDescription(work_desc);
        } catch (CoreException e) {
            e.printStackTrace();
        }

        ResourcesPlugin.getWorkspace().addResourceChangeListener(this, IResourceChangeEvent.POST_CHANGE);

        branchStatusUpdateThread = new BranchStatusUpdateThread();
        branchStatusUpdateThread.start();
    }

	private void updateSocksProxy() {
        IPreferenceStore store = getPreferenceStore();
        String socks_proxy = store.getString(PreferenceConstants.P_SOCKSPROXY);
        int socks_proxy_port = store.getInt(PreferenceConstants.P_SOCKSPROXYPORT);

        if (!socks_proxy.isEmpty()) {
            System.setProperty("socksProxyHost", socks_proxy);
            System.setProperty("socksProxyPort", Integer.toString(socks_proxy_port));
        }
        else {
            System.clearProperty("socksProxyHost");
            System.clearProperty("socksProxyPort");
        }

        IProxyService proxy_service = getProxyService();
        IProxyData[] proxy_data = proxy_service.getProxyData();
        for (IProxyData data : proxy_data) {
            if (data.getType().equals(IProxyData.SOCKS_PROXY_TYPE)) {
                data.setHost(socks_proxy);
                data.setPort(socks_proxy_port);
            }
        }

        proxy_service.setProxiesEnabled(!socks_proxy.isEmpty());
        proxy_service.setSystemProxiesEnabled(false);

        try {
            proxy_service.setProxyData(proxy_data);
        } catch (CoreException e) {
            e.printStackTrace();
        }

	}

	public IClientFactory getClientFactory() {
	    return factory;
	}

	private void connectProjectClient() {
        ClientConfig cc = new DefaultClientConfig();
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        IPreferenceStore store = getPreferenceStore();
        String user = store.getString(PreferenceConstants.P_USERNAME);
        String passwd = store.getString(PreferenceConstants.P_PASSWORD);
        String baseUriString = store.getString(PreferenceConstants.P_SERVER_URI);
        String usersUriString = String.format("%s/users", baseUriString);

        Client client = Client.create(cc);
        factory = new DelegatingClientFactory(new ClientFactory(client));
        RepositoryFileSystemPlugin.setClientFactory(factory);
        client.addFilter(new HTTPBasicAuthFilter(user, passwd));

        IUsersClient usersClient = factory.getUsersClient(UriBuilder.fromUri(usersUriString).build());
        UserInfo userInfo;
        try {
            userInfo = usersClient.getUserInfo(user);
        } catch (RepositoryException e) {
            e.printStackTrace();
            return;
        }
        this.userInfo = userInfo;
        String projectsUriString = String.format("%s/projects/%d", baseUriString, userInfo.getId());

        URI projectsUri;
        projectsUri = UriBuilder.fromUri(projectsUriString).build();

        projectsClient = factory.getProjectsClient(projectsUri);
	}

	void setProjectExplorerInput(Object container) {
        IWorkbenchWindow[] workbenchs = PlatformUI.getWorkbench().getWorkbenchWindows();

        ProjectExplorer view = null;
        for (IWorkbenchWindow workbench : workbenchs) {
            for (IWorkbenchPage page : workbench.getPages()) {
                view = (ProjectExplorer) page
                        .findView("org.eclipse.ui.navigator.ProjectExplorer");
                break;
            }
        }

        if (view != null)
        {
            view.getCommonViewer().setInput(container);
        }
	}

	public void disconnectFromBranch() throws RepositoryException {
	    if (projectClient != null) {
    	    ProjectInfo projectInfo = projectClient.getProjectInfo();
            IProject cr_project = ResourcesPlugin.getWorkspace().getRoot().getProject(projectInfo.getName());
            if (cr_project.exists())
            {
                try {
                    cr_project.delete(true, new NullProgressMonitor());
                    this.project = null;
                    setProjectExplorerInput(ResourcesPlugin.getWorkspace().getRoot());
                } catch (CoreException e) {
                    e.printStackTrace();
                }
            }
	    }

	    this.branchClient = null;
	    this.activeBranch = null;
	}

    public void connectToBranch(IProjectClient projectClient, String branch) throws RepositoryException {
        this.projectClient = projectClient;
        URI uri = ClientUtils.getBranchUri(projectClient, branch);
        this.branchClient = projectClient.getClientFactory().getBranchClient(uri);
        activeBranch = branch;

        try {
            deleteAllCrProjects();
        } catch (CoreException e) {
            DialogUtil.openError("Connect To Branch", "Unable to delete projects", e);
        }

        ProjectInfo projectInfo = projectClient.getProjectInfo();

        this.project = ResourcesPlugin.getWorkspace().getRoot().getProject(projectInfo.getName());
        final IProject p = this.project;

        IProgressService service = PlatformUI.getWorkbench().getProgressService();
        try {
            service.runInUI(service, new IRunnableWithProgress() {

                @Override
                public void run(IProgressMonitor monitor) throws InvocationTargetException,
                        InterruptedException {
                    try {
                        if (p.exists())
                            p.delete(true, monitor);

                        p.create(monitor);
                        p.open(monitor);

                        URI uri = UriBuilder.fromUri(branchClient.getURI()).scheme("crepo").build();
                        EditorUtil.getContentRoot(p).createLink(uri, IResource.REPLACE, monitor);

                        IProjectDescription pd = p.getDescription();
                        pd.setNatureIds(new String[] { "com.dynamo.cr.editor.core.crnature" });
                        ICommand build_command = pd.newCommand();
                        build_command.setBuilderName("com.dynamo.cr.editor.builders.contentbuilder");
                        pd.setBuildSpec(new ICommand[] {build_command});
                        p.setDescription(pd, monitor);
                    } catch (CoreException ex) {
                        DialogUtil.openError("Error occured when creating project", ex.getMessage(), ex);
                    }
                }
            }, null);
        } catch (Throwable e2) {
            DialogUtil.openError("Error occured when creating project", e2.getMessage(), e2);
        }

        setProjectExplorerInput(p.getFolder("content"));
        this.branchStatusUpdateThread.update();
    }

    public URI getBranchURI() {
        if (branchClient == null)
            return null;
        return branchClient.getURI();
    }


	/*
	 * (non-Javadoc)
	 * @see org.osgi.framework.BundleActivator#stop(org.osgi.framework.BundleContext)
	 */
	public void stop(BundleContext bundleContext) throws Exception {
        super.stop(bundleContext);
        ResourcesPlugin.getWorkspace().removeResourceChangeListener(this);
        plugin = null;
		Activator.context = null;
        IPreferenceStore store = getPreferenceStore();
        store.removePropertyChangeListener(this);
        this.branchStatusUpdateThread.dispose();
	}

    @Override
    public void propertyChange(PropertyChangeEvent event) {
        String p = event.getProperty();
        if (p.equals(PreferenceConstants.P_SERVER_URI)) {
            connectProjectClient();
        }
        else if (p.equals(PreferenceConstants.P_USERNAME)) {
            connectProjectClient();
        }
        else if (p.equals(PreferenceConstants.P_PASSWORD)) {
            connectProjectClient();
        }

        else if (p.equals(PreferenceConstants.P_SOCKSPROXY) ||
        		 p.equals(PreferenceConstants.P_SOCKSPROXYPORT)) {
            updateSocksProxy();
        }
    }

    public IBranchClient getBranchClient() {
        return branchClient;
    }

    public static String getPlatform() {
        String os_name = System.getProperty("os.name").toLowerCase();

        if (os_name.indexOf("win") != -1)
            return "win32";
        else if (os_name.indexOf("mac") != -1)
            return "darwin";
        else if (os_name.indexOf("linux") != -1)
            return "linux";
        return null;
    }

    public static void openError(Shell shell, String title, String message, Throwable e) {
        ErrorDialog.openError(shell, title, e.getMessage(),
                new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage(), e));
    }

    @Override
    public void resourceChanged(IResourceChangeEvent event) {
        this.branchStatusUpdateThread.update();
    }


    @Override
    protected void initializeImageRegistry(ImageRegistry reg) {
        super.initializeImageRegistry(reg);

        reg.put(OVERLAY_ERROR_IMAGE_ID, getImageDescriptor("icons/overlay_error.png"));
        reg.put(OVERLAY_EDIT_IMAGE_ID, getImageDescriptor("icons/overlay_edit.png"));
        reg.put(OVERLAY_ADD_IMAGE_ID, getImageDescriptor("icons/overlay_add.png"));
        reg.put(OVERLAY_DELETE_IMAGE_ID, getImageDescriptor("icons/overlay_delete.png"));
        reg.put(UPDATE_LARGE_IMAGE_ID, getImageDescriptor("icons/database_refresh_large.png"));
        reg.put(COMMIT_LARGE_IMAGE_ID, getImageDescriptor("icons/database_save_large.png"));
        reg.put(RESOLVE_LARGE_IMAGE_ID, getImageDescriptor("icons/database_error_large.png"));
        reg.put(DONE_LARGE_IMAGE_ID, getImageDescriptor("icons/database_check_large.png"));
        reg.put(ERROR_LARGE_IMAGE_ID, getImageDescriptor("icons/database_error_large.png"));
        reg.put(UNRESOLVED_IMAGE_ID, getImageDescriptor("icons/arrow_divide_red.png"));
        reg.put(YOURS_IMAGE_ID, getImageDescriptor("icons/user.png"));
        reg.put(THEIRS_IMAGE_ID, getImageDescriptor("icons/group.png"));
    }

    class BranchStatusUpdateThread extends Thread {

        private boolean quit;
        Semaphore semaphore;
        java.util.List<ResourceStatus> resourceStatuses;

        public BranchStatusUpdateThread() {
            super();
            this.quit = false;
            this.semaphore = new Semaphore(0);
            this.resourceStatuses = new java.util.ArrayList<ResourceStatus>();
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
                            BranchStatus branchStatus = client.getBranchStatus();

                            final java.util.List<ResourceStatus> newResourceStatuses = new java.util.ArrayList<ResourceStatus>(branchStatus.getFileStatusCount());
                            for (com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status s : branchStatus.getFileStatusList()) {
                                newResourceStatuses.add(new ResourceStatus(s));
                            }

                            boolean changed = false;
                            if (newResourceStatuses.size() == resourceStatuses.size()) {
                                for (int i = 0; i < resourceStatuses.size(); i++) {
                                    com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status currentStatus = resourceStatuses.get(i).getStatus();
                                    com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status status = newResourceStatuses.get(i).getStatus();
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
                                    for (ResourceStatus s : resourceStatuses) {
                                        IResource resource = s.getResource();
                                        if (resource != null) {
                                            resource.setSessionProperty(BranchStatusDecorator.BRANCH_STATUS_PROPERTY, null);
                                        }
                                    }
                                    for (ResourceStatus s : newResourceStatuses) {
                                        IResource resource = s.getResource();
                                        if (resource != null) {
                                            String status = s.getStatus().getIndexStatus();
                                            if (!s.getStatus().getWorkingTreeStatus().equals(" ")) {
                                                status += s.getStatus().getWorkingTreeStatus();
                                            }
                                            resource.setSessionProperty(BranchStatusDecorator.BRANCH_STATUS_PROPERTY, status);
                                            // TODO: This should make the projectexplorer refresh, not sure how yet
                                        }
                                    }
                                }
                            } catch (CoreException e) {
                                e.printStackTrace();
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
}
