package com.dynamo.cr.editor;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.net.Authenticator;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.nio.file.Files;
import java.util.HashSet;
import java.util.Set;
import javax.ws.rs.core.UriBuilder;

import org.apache.commons.io.FileUtils;
import org.eclipse.core.net.proxy.IProxyData;
import org.eclipse.core.net.proxy.IProxyService;
import org.eclipse.core.resources.ICommand;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IProjectDescription;
import org.eclipse.core.resources.IProjectNature;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceChangeListener;
import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.resources.IResourceDeltaVisitor;
import org.eclipse.core.resources.IWorkspace;
import org.eclipse.core.resources.IWorkspaceDescription;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.eclipse.core.runtime.Status;
import org.eclipse.jetty.server.Server;
import org.eclipse.jetty.server.bio.SocketConnector;
import org.eclipse.jetty.server.handler.HandlerList;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.jface.util.IPropertyChangeListener;
import org.eclipse.jface.util.PropertyChangeEvent;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.IWorkbenchPreferenceConstants;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.navigator.resources.ProjectExplorer;
import org.eclipse.ui.progress.IProgressService;
import org.eclipse.ui.statushandlers.StatusManager;
import org.osgi.framework.BundleContext;
import org.osgi.util.tracker.ServiceTracker;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.bridge.SLF4JBridgeHandler;

import com.dynamo.cr.builtins.Builtins;
import com.dynamo.cr.client.BranchStatusChangedEvent;
import com.dynamo.cr.client.ClientFactory;
import com.dynamo.cr.client.ClientUtils;
import com.dynamo.cr.client.DelegatingClientFactory;
import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.IBranchListener;
import com.dynamo.cr.client.IClientFactory;
import com.dynamo.cr.client.IClientFactory.BranchLocation;
import com.dynamo.cr.client.IProjectClient;
import com.dynamo.cr.client.IProjectsClient;
import com.dynamo.cr.client.IUsersClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.client.filter.DefoldAuthFilter;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.dialogs.OpenIDLoginDialog;
import com.dynamo.cr.editor.fs.RepositoryFileSystemPlugin;
import com.dynamo.cr.editor.preferences.PreferenceConstants;
import com.dynamo.cr.editor.services.IBranchService;
import com.dynamo.cr.editor.ui.AbstractDefoldPlugin;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.rlog.RLogPlugin;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;

public class Activator extends AbstractDefoldPlugin implements IPropertyChangeListener, IResourceChangeListener,
        IBranchListener {

    // The plug-in ID
    public static final String PLUGIN_ID = "com.dynamo.cr.editor"; //$NON-NLS-1$

    // Shared images
    public static final String OVERLAY_ERROR_IMAGE_ID = "OVERLAY_ERROR";
    public static final String OVERLAY_WARNING_IMAGE_ID = "OVERLAY_WARNING";
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
    public static final String LIBRARY_IMAGE_ID = "LIBRARY";

    // The shared instance
    private static Activator plugin;

    private static BundleContext context;

    boolean branchListenerAdded = false;

    public static final int SERVER_PORT = 8080;

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

    private static Logger logger = LoggerFactory.getLogger(Activator.class);

    private IClientFactory factory;

    @SuppressWarnings("rawtypes")
    private ServiceTracker proxyTracker;

    public UserInfo userInfo;

    public IProjectsClient projectsClient;

    private IProject project;

    private Server httpServer;

    public IProxyService getProxyService() {
        return (IProxyService) proxyTracker.getService();
    }

    public IProject getProject() {
        return project;
    }

    void deleteAllCrProjects() throws CoreException {
        IProject[] projects = ResourcesPlugin.getWorkspace().getRoot().getProjects();
        for (IProject p : projects) {
            if (!p.isOpen()) {
                p.open(new NullProgressMonitor());
            }
            IProjectNature nature = p.getNature("com.dynamo.cr.editor.core.crnature");
            if (nature != null) {
                p.delete(true, new NullProgressMonitor());
            }
        }
    }

    public static void logException(Throwable e) {
        Status status = new Status(IStatus.ERROR, PLUGIN_ID, e.getMessage(), e);
        StatusManager.getManager().handle(status, StatusManager.LOG);
    }

    public static void showError(String message, Throwable e) {
        Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, message, e);
        StatusManager.getManager().handle(status, StatusManager.SHOW | StatusManager.LOG | StatusManager.BLOCK);
    }

    /*
     * (non-Javadoc)
     * @see org.osgi.framework.BundleActivator#start(org.osgi.framework.BundleContext)
     */
    @Override
    @SuppressWarnings({ "unchecked", "rawtypes" })
    public void start(BundleContext bundleContext) throws Exception {
        // NOTE: We should probably move parts of the code
        // here to another method post start. If this method
        // throws an exception the editor fails to start
        super.start(bundleContext);
        plugin = this;
        Activator.context = bundleContext;

        // Install java.util.logging to slf4j logging bridge
        // java.util.logging is used by TextureGenerator
        SLF4JBridgeHandler.install();

        IPreferenceStore store = getPreferenceStore();
        if (store.getBoolean(PreferenceConstants.P_ANONYMOUS_LOGGING) && !EditorUtil.isDev()) {
            RLogPlugin.getDefault().startLogging();
        }

        /*
         * Ensure that SplashHandler#getBundleProgressMonitor is called.
         * If we set org.eclipse.ui/SHOW_PROGRESS_ON_STARTUP = true in plugin_customization.ini
         * *and* uncheck "add a progress bar" in the product eclipse will change the SHOW_PROGRESS_ON_STARTUP
         * to false during export.
         *
         * See for a related issue: https://bugs.eclipse.org/bugs/show_bug.cgi?id=189950
         */
        PlatformUI.getPreferenceStore().setValue(IWorkbenchPreferenceConstants.SHOW_PROGRESS_ON_STARTUP, true);

        // We clear this property in order to avoid the following warning on OSX:
        // "System property http.nonProxyHosts has been set to local|*.local|169.254/16|*.169.254/16 by an external source. This value will be overwritten using the values from the preferences"
        System.clearProperty("http.nonProxyHosts");

        proxyTracker = new ServiceTracker(bundleContext, IProxyService.class
                .getName(), null);
        proxyTracker.open();

        //connectProjectClient();
        store.addPropertyChangeListener(this);
        // TODO This is a hack to make sure noone is using remote branches, which is not currently supported
        store.setValue(PreferenceConstants.P_USE_LOCAL_BRANCHES, true);
        updateSocksProxy();

        // Extract and set updated SSL cacerts file
        // The cacerts file is extracted into <install-directory>/configuration/cacerts
        // This is done to circumvent usage of any old cacerts file that is bundled with the old JRE we ship with Defold.
        // (The current JRE has been updated to use a new cacerts file, but for old users that simply update the application
        //  will not get a new JRE, and thus be stuck with an old cacerts file.)
        String installLoaction = Platform.getInstallLocation().getURL().getPath();
        IPath configurationDirPath = new Path(installLoaction).append("configuration");
        File configurationDir = new File(configurationDirPath.toOSString());
        configurationDir.mkdirs();
        File cacertsFile = new File(configurationDir, "cacerts");
        URL input = getClass().getClassLoader().getResource("/cacerts");
        FileUtils.copyURLToFile(input, cacertsFile);
        System.setProperty("javax.net.ssl.trustStore", cacertsFile.getAbsolutePath());

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
        /*
         * NOTE: We're using HTTP Basic Auth for Git over HTTP
         * Eclipse will popup a dialog for some reason. Could be that JGit
         * only authenticate when "asked", ie when UNAUTHORIZED is returned.
         * In that case eclipse will catch that and show a dialog. Using
         * HTTP Basic Auth is a hack due to limitations in JGit. No support for custom HTTP
         * headers. We should probably patch JGit and remove the line below at some point.
         */
        Authenticator.setDefault(null);

        /*
         * NOTE: We can't invoke getWorkbench() in start. The workbench is not started yet.
         * Should we really use workspace services for this? (IBranchClient)
         */
        if (!branchListenerAdded) {
            IBranchService branchService = (IBranchService)PlatformUI.getWorkbench().getService(IBranchService.class);
            if (branchService != null) {
                branchListenerAdded = true;
                branchService.addBranchListener(this);
            } else {
                Status status = new Status(IStatus.ERROR, PLUGIN_ID, "Unable to locate IBranchService");
                StatusManager.getManager().handle(status, StatusManager.LOG);
            }
        }

        ClientConfig cc = new DefaultClientConfig();
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
        cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

        IPreferenceStore store = getPreferenceStore();
        String email = store.getString(PreferenceConstants.P_EMAIL);
        String authToken = store.getString(PreferenceConstants.P_AUTH_TOKEN);
        String baseUriString = store.getString(PreferenceConstants.P_SERVER_URI);
        String usersUriString = String.format("%s/users", baseUriString);

        DefoldAuthFilter authFilter = new DefoldAuthFilter(email, authToken, null);
        Client client = Client.create(cc);
        client.addFilter(authFilter);
        BranchLocation branchLocation;
        String branchRoot = null;
        if (store.getBoolean(PreferenceConstants.P_USE_LOCAL_BRANCHES)) {
            branchLocation = BranchLocation.LOCAL;
            // TODO: Use getInstallLocation() or not?
            // What happens when the application is installed? Use home-dir instead? Configurable?
            String location = Platform.getInstallLocation().getURL().getPath();
            IPath branchRootPath = new Path(location).append("branches");
            new File(branchRootPath.toOSString()).mkdirs();
            branchRoot = branchRootPath.toPortableString();
        } else {
            branchLocation = BranchLocation.REMOTE;
        }
        factory = new DelegatingClientFactory(new ClientFactory(client, branchLocation, branchRoot, email, authToken));
        RepositoryFileSystemPlugin.setClientFactory(factory);

        boolean validAuthToken = false;
        if (email != null && !email.isEmpty() && authToken != null && !authToken.isEmpty()) {
            // Try to validate auth-cookie
            IUsersClient usersClient = factory.getUsersClient(UriBuilder.fromUri(usersUriString).build());

            try {
                @SuppressWarnings("unused")
                UserInfo userInfo = usersClient.getUserInfo(email);
                validAuthToken = true;
            } catch (RepositoryException e) {
                validAuthToken = false;
            }
        }

        Shell shell = Display.getDefault().getActiveShell();
        if (!validAuthToken) {
            OpenIDLoginDialog openIDdialog = new OpenIDLoginDialog(shell, baseUriString);
            int ret = openIDdialog.open();
            if (ret == Dialog.CANCEL) {
                return false;
            }
            email = openIDdialog.getEmail();
            authToken = openIDdialog.getAuthToken();
            authFilter.setEmail(email);
            authFilter.setauthToken(authToken);

            // Recreate factory since we now have credentials
            factory = new DelegatingClientFactory(new ClientFactory(client, branchLocation, branchRoot, email, authToken));
            RepositoryFileSystemPlugin.setClientFactory(factory);

            store.setValue(PreferenceConstants.P_EMAIL, email);
            store.setValue(PreferenceConstants.P_AUTH_TOKEN, authToken);
        }

        IUsersClient usersClient = factory.getUsersClient(UriBuilder.fromUri(usersUriString).build());
        UserInfo userInfo;
        try {
            userInfo = usersClient.getUserInfo(email);
        } catch (RepositoryException e) {
            final String message = "Error signing in";
            Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, message, e);
            StatusManager.getManager().handle(status, StatusManager.SHOW | StatusManager.LOG);
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
        if (httpServer != null) {
            try {
                httpServer.stop();
            } catch (Exception e) {
                logger.warn("Failed to stop http server", e);
            }
            httpServer = null;
        }

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
                    branchService.updateBranchStatus(null);
                }
            }
        }

        this.branchClient = null;
        this.activeBranch = null;
    }

    private void initHttpServer(String branchLocation) throws IOException {
        httpServer = new org.eclipse.jetty.server.Server();

        SocketConnector connector = new SocketConnector();
        connector.setPort(SERVER_PORT);
        httpServer.addConnector(connector);
        HandlerList handlerList = new HandlerList();
        FileHandler fileHandler = new FileHandler();
        fileHandler.setResourceBase(branchLocation);
        handlerList.addHandler(fileHandler);
        httpServer.setHandler(handlerList);

        try {
            httpServer.start();
        } catch (Exception e) {
            throw new IOException("Unable to start http server", e);
        }
    }

    public void connectToBranch(IProjectClient projectClient, String branch) throws RepositoryException {

        try {
            // Disable the link overlay of the icons in the project explorer
            PlatformUI.getWorkbench().getDecoratorManager().setEnabled("org.eclipse.ui.LinkedResourceDecorator", false);
        } catch (CoreException e1) {
            // Ignore exceptions, decoration only
        }

        this.projectClient = projectClient;
        URI uri = ClientUtils.getBranchUri(projectClient, branch);
        this.branchClient = projectClient.getClientFactory().getBranchClient(uri);
        activeBranch = branch;

        try {
            deleteAllCrProjects();
        } catch (CoreException e) {
            Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, "Error occurred while removing old project data.", e);
            StatusManager.getManager().handle(status, StatusManager.SHOW | StatusManager.LOG);
        }

        ProjectInfo projectInfo = projectClient.getProjectInfo();

        this.project = ResourcesPlugin.getWorkspace().getRoot().getProject(projectInfo.getName());
        final IProject p = this.project;

        IPreferenceStore store = getPreferenceStore();
        final String email = store.getString(PreferenceConstants.P_EMAIL);
        final String authToken = store.getString(PreferenceConstants.P_AUTH_TOKEN);
        final boolean useLocalBranches = store.getBoolean(PreferenceConstants.P_USE_LOCAL_BRANCHES);

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
                        p.setDefaultCharset("UTF-8", monitor);

                        URI uri = UriBuilder.fromUri(branchClient.getURI()).scheme("crepo").build();

                        IFolder contentRoot = EditorUtil.getContentRoot(p);
                        contentRoot.createLink(uri, IResource.REPLACE, monitor);

                        IProjectDescription pd = p.getDescription();
                        pd.setNatureIds(new String[] { "com.dynamo.cr.editor.core.crnature" });
                        ICommand build_command = pd.newCommand();
                        build_command.setBuilderName("com.dynamo.cr.editor.builders.contentbuilder");
                        pd.setBuildSpec(new ICommand[] {build_command});
                        p.setDescription(pd, monitor);

                        IFolder internal = contentRoot.getFolder(".internal");
                        if (!internal.exists()) {
                            internal.create(true, true, monitor);
                        }
                        try {
                            BobUtil.resolveLibs(contentRoot, email, authToken, monitor);
                        } catch (CoreException e) {
                            showError("Error occurred when fetching libraries", e);
                        }
                        try {
                            linkBuiltins(contentRoot, monitor);
                        } catch (CoreException e) {
                            showError("Error occurred when linking builtins", e);
                        }
                    } catch (CoreException ex) {
                        showError("Error occurred when creating project", ex);
                    }
                }
            }, null);
        } catch (Throwable e2) {
            showError("Error occured when creating project", e2);
        }

        if (useLocalBranches) {
            String branchLocation = branchClient.getNativeLocation();
            File dest = new File(new Path(branchLocation).append("builtins").toOSString());
            if (dest.exists()) {
                try {
                    FileUtils.deleteDirectory(dest);
                } catch (IOException e) {
                    showError("Could not delete builtins-directory, old resources might remain.", e);
                }
            }

            // Start local http server
            try {
                initHttpServer(branchLocation);
            } catch (IOException e) {
                showError("Unable to start http server", e);
            }
        }

        setProjectExplorerInput(p.getFolder("content"));
        IBranchService branchService = (IBranchService)PlatformUI.getWorkbench().getService(IBranchService.class);
        if (branchService != null) {
            branchService.updateBranchStatus(null);
        }
    }

    private void linkBuiltins(IFolder contentRoot, IProgressMonitor monitor) throws CoreException {
        try {
            URI uri = new URI("bundle", null, "/builtins/", Builtins.getDefault().getBundle().getSymbolicName(), null);
            IFolder folder = contentRoot.getFolder("builtins");
            folder.createLink(uri, IResource.VIRTUAL | IResource.REPLACE | IResource.ALLOW_MISSING_LOCAL, monitor);
        } catch (URISyntaxException e) {
            throw new CoreException(new Status(IStatus.ERROR, PLUGIN_ID, e.getMessage(), e));
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
    @Override
    public void stop(BundleContext bundleContext) throws Exception {
        super.stop(bundleContext);
        ResourcesPlugin.getWorkspace().removeResourceChangeListener(this);
        plugin = null;
        Activator.context = null;
        IPreferenceStore store = getPreferenceStore();
        store.removePropertyChangeListener(this);

        IBranchService branchService = (IBranchService)PlatformUI.getWorkbench().getService(IBranchService.class);
        if (branchService != null) {
            branchService.removeBranchListener(this);
        }
    }

    @Override
    public void propertyChange(PropertyChangeEvent event) {
        String p = event.getProperty();
        if (p.equals(PreferenceConstants.P_SERVER_URI)) {
            connectProjectClient();
        } else if (p.equals(PreferenceConstants.P_USE_LOCAL_BRANCHES)) {
            connectProjectClient();
        } else if (p.equals(PreferenceConstants.P_SOCKS_PROXY) ||
                p.equals(PreferenceConstants.P_SOCKS_PROXY_PORT)) {
            updateSocksProxy();
        } else if (p.equals(PreferenceConstants.P_ANONYMOUS_LOGGING)) {
            IPreferenceStore store = getPreferenceStore();
            boolean log = store.getBoolean(PreferenceConstants.P_ANONYMOUS_LOGGING);
            if (log && !EditorUtil.isDev()) {
                RLogPlugin.getDefault().startLogging();
            } else {
                RLogPlugin.getDefault().stopLogging();
            }
        }
    }

    public IBranchClient getBranchClient() {
        return branchClient;
    }

    @Override
    public void resourceChanged(IResourceChangeEvent event) {
        // Resource changed events are not guaranteed to carry a delta
        // Ignore such events since we are only interested in deltas
        if (event.getDelta() == null) {
            return;
        }
        // Ignore resource changed while the application is launching
        if (!PlatformUI.isWorkbenchRunning()) {
            return;
        }
        final IBranchService branchService = (IBranchService)PlatformUI.getWorkbench().getService(IBranchService.class);
        if (branchService == null) {
            return;
        }

        // Mask of interesting resource kind change flags
        final int mask = IResourceDelta.ADDED
                | IResourceDelta.REMOVED
                | IResourceDelta.CHANGED;

        // Set of changed resources
        final Set<IResource> changedResources = new HashSet<IResource>();

        try {
            event.getDelta().accept(new IResourceDeltaVisitor() {
                @Override
                public boolean visit(IResourceDelta delta) throws CoreException {
                    if ((delta.getKind() & mask) != 0) {
                        IResource resource = delta.getResource();
                        if (resource != null) {
                            changedResources.add(delta.getResource());
                        }
                    }
                    return true;
                }
            });
        } catch (CoreException e) {
            Activator.logException(e);
        }

        if (changedResources.size() > 0) {
            branchService.updateBranchStatus(changedResources);
        }
    }

    @Override
    protected void initializeImageRegistry(ImageRegistry reg) {
        super.initializeImageRegistry(reg);

        reg.put(OVERLAY_ERROR_IMAGE_ID, getImageDescriptor("icons/overlay_error.png"));
        reg.put(OVERLAY_WARNING_IMAGE_ID, getImageDescriptor("icons/overlay_warning.png"));
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
        reg.put(LIBRARY_IMAGE_ID, getImageDescriptor("icons/plugin.png"));
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
                        projectExplorer.getCommonViewer().update(resources, null);
                    }
                });
            }
        }
    }

}
