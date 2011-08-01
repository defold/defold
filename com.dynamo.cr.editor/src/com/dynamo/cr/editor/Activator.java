package com.dynamo.cr.editor;

import java.lang.reflect.InvocationTargetException;
import java.net.URI;
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
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.dialogs.ErrorDialog;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.jface.util.IPropertyChangeListener;
import org.eclipse.jface.util.PropertyChangeEvent;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.navigator.resources.ProjectExplorer;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.eclipse.ui.progress.IProgressService;
import org.eclipse.ui.statushandlers.StatusManager;
import org.osgi.framework.BundleContext;
import org.osgi.util.tracker.ServiceTracker;

import com.dynamo.cr.client.BranchStatusChangedEvent;
import com.dynamo.cr.client.ClientFactory;
import com.dynamo.cr.client.ClientUtils;
import com.dynamo.cr.client.DelegatingClientFactory;
import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.IBranchListener;
import com.dynamo.cr.client.IClientFactory;
import com.dynamo.cr.client.IProjectClient;
import com.dynamo.cr.client.IProjectsClient;
import com.dynamo.cr.client.IUsersClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.client.filter.DefoldAuthFilter;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.dialogs.DialogUtil;
import com.dynamo.cr.editor.dialogs.OpenIDLoginDialog;
import com.dynamo.cr.editor.fs.RepositoryFileSystemPlugin;
import com.dynamo.cr.editor.preferences.PreferenceConstants;
import com.dynamo.cr.editor.services.IBranchService;
import com.dynamo.cr.markers.BranchStatusMarker;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;

public class Activator extends AbstractUIPlugin implements IPropertyChangeListener, IResourceChangeListener, IBranchListener {

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

    public static void logException(Throwable e) {
        Status status = new Status(IStatus.ERROR, PLUGIN_ID, e.getMessage(), e);
        StatusManager.getManager().handle(status, StatusManager.LOG);
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
		//connectProjectClient();
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
            Activator.logException(e);
        }

        ResourcesPlugin.getWorkspace().addResourceChangeListener(this, IResourceChangeEvent.POST_CHANGE);
    }

	private void updateSocksProxy() {
        IPreferenceStore store = getPreferenceStore();
        String socks_proxy = store.getString(PreferenceConstants.P_SOCKS_PROXY);
        int socks_proxy_port = store.getInt(PreferenceConstants.P_SOCKS_PROXY_PORT);

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
            Activator.logException(e);
        }

	}

	public IClientFactory getClientFactory() {
	    return factory;
	}

	public boolean connectProjectClient() {
        ClientConfig cc = new DefaultClientConfig();
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        IPreferenceStore store = getPreferenceStore();
        String email = store.getString(PreferenceConstants.P_EMAIL);
        String authCookie = store.getString(PreferenceConstants.P_AUTH_COOKIE);
        String baseUriString = store.getString(PreferenceConstants.P_SERVER_URI);
        String usersUriString = String.format("%s/users", baseUriString);

        DefoldAuthFilter authFilter = new DefoldAuthFilter(email, authCookie, null);
        Client client = Client.create(cc);
        client.addFilter(authFilter);
        factory = new DelegatingClientFactory(new ClientFactory(client));
        RepositoryFileSystemPlugin.setClientFactory(factory);

        boolean validAuthCookie = false;
        if (email != null && authCookie != null) {
            // Try to validate auth-cookie
            IUsersClient usersClient = factory.getUsersClient(UriBuilder.fromUri(usersUriString).build());

            try {
                @SuppressWarnings("unused")
                UserInfo userInfo = usersClient.getUserInfo(email);
                validAuthCookie = true;
            } catch (RepositoryException e) {
                validAuthCookie = false;
            }
        }

        Shell shell = Display.getDefault().getActiveShell();
        if (!validAuthCookie) {
            OpenIDLoginDialog openIDdialog = new OpenIDLoginDialog(shell, baseUriString);
            int ret = openIDdialog.open();
            if (ret == Dialog.CANCEL) {
                return false;
            }
            email = openIDdialog.getEmail();

            authFilter.setEmail(email);
            authFilter.setAuthCookie(openIDdialog.getAuthCookie());
            store.setValue(PreferenceConstants.P_EMAIL, email);
            store.setValue(PreferenceConstants.P_AUTH_COOKIE, openIDdialog.getAuthCookie());
        }

        IUsersClient usersClient = factory.getUsersClient(UriBuilder.fromUri(usersUriString).build());
        UserInfo userInfo;
        try {
            userInfo = usersClient.getUserInfo(email);
        } catch (RepositoryException e) {
            final String message = "Error signing in";
            ErrorDialog.openError(shell, message, message,
                    new Status(IStatus.ERROR, Activator.PLUGIN_ID, message));
            return false;
        }
        this.userInfo = userInfo;
        String projectsUriString = String.format("%s/projects/%d", baseUriString, userInfo.getId());

        URI projectsUri;
        projectsUri = UriBuilder.fromUri(projectsUriString).build();

        projectsClient = factory.getProjectsClient(projectsUri);

        return true;
	}

	void setProjectExplorerInput(Object container) {
        ProjectExplorer view = findProjectExplorer();
        if (view != null) {
            view.getCommonViewer().setInput(container);
        }
	}

	public ProjectExplorer findProjectExplorer() {
        ProjectExplorer view = null;
        IWorkbenchWindow[] workbenches = PlatformUI.getWorkbench().getWorkbenchWindows();
        for (IWorkbenchWindow workbench : workbenches) {
            for (IWorkbenchPage page : workbench.getPages()) {
                view = (ProjectExplorer) page
                        .findView("org.eclipse.ui.navigator.ProjectExplorer");
                break;
            }
        }
        return view;
	}

	public void disconnectFromBranch() throws RepositoryException {
	    if (projectClient != null) {
    	    ProjectInfo projectInfo = projectClient.getProjectInfo();
            IProject cr_project = ResourcesPlugin.getWorkspace().getRoot().getProject(projectInfo.getName());
            if (cr_project.exists())
            {
                try {
                    this.project = null;
                    cr_project.delete(true, new NullProgressMonitor());
                    setProjectExplorerInput(ResourcesPlugin.getWorkspace().getRoot());
                } catch (CoreException e) {
                    Activator.logException(e);
                }
                IBranchService branchService = (IBranchService)PlatformUI.getWorkbench().getService(IBranchService.class);
                if (branchService != null) {
                    branchService.updateBranchStatus();
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
        IBranchService branchService = (IBranchService)PlatformUI.getWorkbench().getService(IBranchService.class);
        if (branchService != null) {
            branchService.updateBranchStatus();
        }
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
	}

    @Override
    public void propertyChange(PropertyChangeEvent event) {
        String p = event.getProperty();
        if (p.equals(PreferenceConstants.P_SERVER_URI)) {
            connectProjectClient();
        }
        else if (p.equals(PreferenceConstants.P_SOCKS_PROXY) ||
        		 p.equals(PreferenceConstants.P_SOCKS_PROXY_PORT)) {
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
        IBranchService branchService = (IBranchService)PlatformUI.getWorkbench().getService(IBranchService.class);
        if (branchService != null) {
            branchService.updateBranchStatus();
        }
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

    @Override
    public void branchStatusChanged(BranchStatusChangedEvent event) {
        final Object[] resources = event.getResources();
        if (resources.length > 0) {
            final ProjectExplorer projectExplorer = Activator.getDefault().findProjectExplorer();
            if (projectExplorer != null) {
                Display.getDefault().asyncExec(new Runnable() {

                    @Override
                    public void run() {
                        projectExplorer.getCommonViewer().update(resources, new String[] {BranchStatusMarker.MARKER_ID});
                    }
                });
            }
        }
    }
}
