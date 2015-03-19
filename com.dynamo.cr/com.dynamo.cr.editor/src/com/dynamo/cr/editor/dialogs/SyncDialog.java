package com.dynamo.cr.editor.dialogs;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;

import org.eclipse.core.commands.Command;
import org.eclipse.core.commands.IParameter;
import org.eclipse.core.commands.Parameterization;
import org.eclipse.core.commands.ParameterizedCommand;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.OperationCanceledException;
import org.eclipse.core.runtime.jobs.Job;
import org.eclipse.jface.dialogs.DialogPage;
import org.eclipse.jface.dialogs.IDialogConstants;
import org.eclipse.jface.dialogs.ProgressIndicator;
import org.eclipse.jface.dialogs.TitleAreaDialog;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.ColumnLabelProvider;
import org.eclipse.jface.viewers.DoubleClickEvent;
import org.eclipse.jface.viewers.IDoubleClickListener;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.TableViewer;
import org.eclipse.jface.viewers.TableViewerColumn;
import org.eclipse.swt.SWT;
import org.eclipse.swt.custom.BusyIndicator;
import org.eclipse.swt.custom.SashForm;
import org.eclipse.swt.custom.StackLayout;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.ModifyEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.events.TraverseEvent;
import org.eclipse.swt.events.TraverseListener;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Group;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Text;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.commands.ICommandService;
import org.eclipse.ui.console.ConsolePlugin;
import org.eclipse.ui.console.IConsole;
import org.eclipse.ui.console.IConsoleManager;
import org.eclipse.ui.console.MessageConsole;
import org.eclipse.ui.console.MessageConsoleStream;
import org.eclipse.ui.handlers.IHandlerService;
import org.eclipse.ui.progress.IProgressService;

import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.BobUtil;
import com.dynamo.cr.editor.compare.BranchStatusTableViewer;
import com.dynamo.cr.editor.compare.ConflictedResourceStatus;
import com.dynamo.cr.editor.compare.ConflictedResourceStatus.IResolveListener;
import com.dynamo.cr.editor.compare.ConflictedResourceStatus.Resolve;
import com.dynamo.cr.editor.compare.ResourceStatus;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.preferences.PreferenceConstants;
import com.dynamo.cr.editor.services.IBranchService;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status;
import com.dynamo.cr.protocol.proto.Protocol.CommitDesc;
import com.dynamo.cr.protocol.proto.Protocol.Log;

public class SyncDialog extends TitleAreaDialog {

    private enum State { UPDATE, COMMIT, PULL, RESOLVE, PUSH, RESOLVE_LIBRARIES }
    private static final int STATE_COUNT = 6;
    private static final String[] STATE_TITLES = { "Update", "Commit", "Pull", "Resolve", "Push", "Resolve Libraries" };
    private static final String[] OK_TITLES = { "OK", "&Commit", "OK", "&Resolve", "OK", "OK" };
    private static final String[] STATE_IMAGE_IDS = {
        Activator.UPDATE_LARGE_IMAGE_ID,
        Activator.COMMIT_LARGE_IMAGE_ID,
        Activator.UPDATE_LARGE_IMAGE_ID,
        Activator.RESOLVE_LARGE_IMAGE_ID,
        Activator.UPDATE_LARGE_IMAGE_ID,
        Activator.RESOLVE_LARGE_IMAGE_ID
        };
    private static final String[] STATE_MESSAGES = {
        "Please wait while information from the server is being retrieved.",
        "Please specify a message and commit your changes. Double click the files to review the changes before commiting.",
        "Please wait while other commits are pulled from the master branch.",
        "There have been commits made that conflicted with your local changes. Please specify which version to keep for each file to resolve the conflicts. Double click the files to review the conflicts.",
        "Please wait while your changes are pushed to the master branch.",
        "Please wait while library dependencies are updated."
        };
    private static final boolean[] STATE_AUTO_PROGRESS = {
        true,
        false,
        true,
        false,
        true,
        true,
        false,
        false
    };

    private State state;
    private SyncDialogPage[] pages;
    private Composite pageComposite;
    private StackLayout stackLayout;
    private String hardBaseRevision;
    private String softBaseRevision;
    private BranchStatus branchStatus;

    public SyncDialog(Shell parentShell) {
        super(parentShell);

        this.pages = new SyncDialogPage[STATE_COUNT];
        this.pages[0] = new UpdateDialogPage(State.values()[0], this);
        this.pages[1] = new CommitDialogPage(State.values()[1], this);
        this.pages[2] = new PullDialogPage(State.values()[2], this);
        this.pages[3] = new ResolveDialogPage(State.values()[3], this);
        this.pages[4] = new PushDialogPage(State.values()[4], this);
        this.pages[5] = new ResolveLibrariesPage(State.values()[5], this);
    }

    public void setState(State state) {
        if (!this.getShell().isDisposed() && this.state != state) {
            if (this.state != null) {
                this.pages[this.state.ordinal()].onHide();
            }
            this.state = state;
            SyncDialogPage page = pages[this.state.ordinal()];
            this.stackLayout.topControl = page.getControl();
            Button okButton = getButton(IDialogConstants.OK_ID);
            if (STATE_AUTO_PROGRESS[this.state.ordinal()]) {
                okButton.setEnabled(false);
            } else {
                okButton.setEnabled(true);
                getShell().setDefaultButton(okButton);
            }
            okButton.setText(OK_TITLES[this.state.ordinal()]);
            setTitle(page.getTitle());
            setTitleImage(page.getImage());
            setMessage(page.getMessage(), page.getMessageType());
            page.onShow();
            this.pageComposite.layout();
        }
    }

    public BranchStatus getBranchStatus() {
        return this.branchStatus;
    }

    public BranchStatus updateBranchStatus() throws RepositoryException {
        IBranchClient branchClient = Activator.getDefault().getBranchClient();
        this.branchStatus = branchClient.getBranchStatus(true);
        return this.branchStatus;
    }

    public void setBranchStatus(BranchStatus branchStatus) {
        this.branchStatus = branchStatus;
    }

    public String getHardBaseRevision() {
        return this.hardBaseRevision;
    }

    public void setHardBaseRevision(String hardBaseRevision) {
        this.hardBaseRevision = hardBaseRevision;
    }

    public String getSoftBaseRevision() {
        return this.softBaseRevision;
    }

    public void setSoftBaseRevision(String softBaseRevision) {
        this.softBaseRevision = softBaseRevision;
    }

    @Override
    protected void configureShell(Shell newShell) {
        setShellStyle(SWT.TITLE | SWT.MIN | SWT.MAX | SWT.CLOSE | SWT.RESIZE | SWT.APPLICATION_MODAL);
        super.configureShell(newShell);
        newShell.setText("Synchronize");
    }

    @Override
    protected Control createContents(Composite parent) {
        Control contents = super.createContents(parent);

        Button okButton = getButton(IDialogConstants.OK_ID);
        okButton.setEnabled(false);

        setState(State.UPDATE);

        return contents;
    }

    @Override
    protected Control createDialogArea(Composite parent) {
        Composite dialogArea = (Composite)super.createDialogArea(parent);
        this.pageComposite = new Composite(dialogArea, SWT.NONE);
        this.stackLayout = new StackLayout();
        this.pageComposite.setLayout(this.stackLayout);
        this.pageComposite.setLayoutData(new GridData(GridData.FILL_BOTH));
        for (int i = 0; i < STATE_COUNT; ++i) {
            pages[i].createControl(this.pageComposite);
        }
        return this.dialogArea;
    }

    @Override
    protected void buttonPressed(int buttonId) {
        switch (buttonId) {
        case IDialogConstants.OK_ID:
            if (this.pages[this.state.ordinal()].onOk()) {
                this.pages[this.state.ordinal()].onHide();
                super.buttonPressed(buttonId);
            }
            break;
        case IDialogConstants.CANCEL_ID:
            if (this.pages[this.state.ordinal()].onCancel()) {
                this.pages[this.state.ordinal()].onHide();
                rollback();
                super.buttonPressed(buttonId);
            }
            break;
        default:
            super.buttonPressed(buttonId);
            break;
        }
    }

    protected void handleShellCloseEvent() {
        if (this.pages[this.state.ordinal()].onCancel()) {
            rollback();
            super.handleShellCloseEvent();
        }
    }

    private MessageConsole findConsole(String name) {
        ConsolePlugin plugin = ConsolePlugin.getDefault();
        IConsoleManager conMan = plugin.getConsoleManager();
        IConsole[] existing = conMan.getConsoles();
        for (int i = 0; i < existing.length; i++)
            if (name.equals(existing[i].getName()))
                return (MessageConsole) existing[i];
        // no console found, so create a new one
        MessageConsole myConsole = new MessageConsole(name, null);
        conMan.addConsoles(new IConsole[] { myConsole });
        return myConsole;
    }

    private void rollback() {
        if (this.hardBaseRevision != null || this.softBaseRevision != null) {
            Job rollbackJob = new Job("Rollback") {

                @Override
                protected IStatus run(IProgressMonitor monitor) {
                    IBranchClient branchClient = Activator.getDefault().getBranchClient();
                    try {
                        if (hardBaseRevision != null && softBaseRevision != null) {
                            monitor.beginTask("Rollback", 2);
                        } else {
                            monitor.beginTask("Rollback", 1);
                        }
                        if (hardBaseRevision != null) {
                            branchClient.reset("hard", hardBaseRevision);
                            monitor.worked(1);
                        }
                        if (softBaseRevision != null) {
                            branchClient.reset("soft", softBaseRevision);
                            monitor.worked(1);
                        }
                    } catch (RepositoryException e) {
                        MessageConsole console = findConsole("console");
                        MessageConsoleStream stream = console.newMessageStream();
                        stream.println(e.getMessage());
                        try {
                            stream.close();
                        } catch (IOException e1) {
                            Activator.logException(e1);
                        }
                    }
                    return org.eclipse.core.runtime.Status.OK_STATUS;
                }
            };
            rollbackJob.setUser(true);
            rollbackJob.schedule();
        }
    }

    private class SyncDialogPage extends DialogPage {

        protected SyncDialog syncDialog;

        public SyncDialogPage(State state, SyncDialog syncDialog) {
            super(STATE_TITLES[state.ordinal()]);
            this.syncDialog = syncDialog;
            setImageDescriptor(Activator.getDefault().getImageRegistry().getDescriptor(STATE_IMAGE_IDS[state.ordinal()]));
            setMessage(STATE_MESSAGES[state.ordinal()], INFORMATION);
        }

        @Override
        public void createControl(Composite parent) {
            Composite composite = new Composite(parent, SWT.NONE);
            composite.setLayout(new GridLayout());
            composite.setLayoutData(new GridData(GridData.FILL_BOTH));
            composite.setFont(parent.getFont());
            createContents(composite);
            composite.pack();
            setControl(composite);
        }

        protected void createContents(Composite parent) {}

        public void onShow() {}
        public void onHide() {}
        public boolean onOk() { return true; }
        public boolean onCancel() { return true; }

        protected void setPageComplete(boolean complete) {
            Button okButton = this.syncDialog.getButton(IDialogConstants.OK_ID);
            okButton.setEnabled(complete);
            if (complete) {
                getShell().setDefaultButton(okButton);
            }
        }

        protected void resolveLibraries() {
            this.syncDialog.setState(State.RESOLVE_LIBRARIES);
        }
    }

    private class ProgressDialogPage extends SyncDialogPage {

        protected boolean cancelled;
        protected ProgressIndicator indicator;

        public ProgressDialogPage(State state, final SyncDialog syncDialog) {
            super(state, syncDialog);
            this.cancelled = false;
        }

        @Override
        protected void createContents(final Composite parent) {
            this.indicator = new ProgressIndicator(parent);
            this.indicator.setLayoutData(new GridData(GridData.FILL_HORIZONTAL));
        }

        @Override
        public boolean onCancel() {
            this.cancelled = true;
            return true;
        }

    }

    private class UpdateDialogPage extends ProgressDialogPage {

        public UpdateDialogPage(State state, final SyncDialog syncDialog) {
            super(state, syncDialog);
        }

        @Override
        public void onShow() {
            this.indicator.beginAnimatedTask();
            final Job job = new Job("Fetch Branch Status") {
                protected IStatus run(IProgressMonitor monitor) {
                    try {
                        final BranchStatus branchStatus = syncDialog.updateBranchStatus();
                        if (!cancelled) {
                            Display.getDefault().asyncExec(new Runnable() {

                                @Override
                                public void run() {
                                    switch (branchStatus.getBranchState()) {
                                    case DIRTY:
                                        syncDialog.setState(State.COMMIT);
                                        break;
                                    case MERGE:
                                        syncDialog.setState(State.RESOLVE);
                                        break;
                                    default:
                                        if (branchStatus.getCommitsBehind() > 0) {
                                            syncDialog.setState(State.PULL);
                                        } else if (branchStatus.getCommitsAhead() > 0){
                                            syncDialog.setState(State.PUSH);
                                        } else {
                                            syncDialog.setMessage("No project changes to synchronize.");
                                            syncDialog.getButton(IDialogConstants.CANCEL_ID).setEnabled(false);
                                            resolveLibraries();
                                        }
                                        break;
                                    }
                                }
                            });
                            return org.eclipse.core.runtime.Status.OK_STATUS;
                        }
                    } catch (RepositoryException e) {
                        final String message = e.getMessage();
                        Display.getDefault().asyncExec(new Runnable() {

                            @Override
                            public void run() {
                                indicator.setVisible(false);
                                syncDialog.setErrorMessage(message);
                            }
                        });
                    }
                    return org.eclipse.core.runtime.Status.CANCEL_STATUS;
                }
            };
            job.setSystem(true);
            job.schedule();
        }

    }

    private class PullDialogPage extends ProgressDialogPage {

        public PullDialogPage(State state, final SyncDialog syncDialog) {
            super(state, syncDialog);
        }

        @Override
        public void onShow() {
            this.indicator.beginAnimatedTask();
            final Job job = new Job("Pull From Master") {
                protected IStatus run(IProgressMonitor monitor) {
                    try {
                        IBranchClient branchClient = Activator.getDefault().getBranchClient();
                        BranchStatus branchStatus = syncDialog.getBranchStatus();
                        boolean done = false;
                        if (!cancelled && branchStatus.getCommitsBehind() > 0) {
                            if (syncDialog.getHardBaseRevision() == null) {
                                Log log = branchClient.log(1);
                                syncDialog.setHardBaseRevision(log.getCommits(0).getId());
                            }
                            branchStatus = branchClient.update();
                            syncDialog.setBranchStatus(branchStatus);
                            if (branchStatus.getFileStatusCount() > 0) {
                                Display.getDefault().asyncExec(new Runnable() {

                                    @Override
                                    public void run() {
                                        syncDialog.setState(State.RESOLVE);
                                    }
                                });
                            } else if (branchStatus.getCommitsAhead() == 0) {
                                done = true;
                            }
                        }
                        if (!cancelled && branchStatus.getFileStatusCount() == 0 && branchStatus.getCommitsAhead() > 0) {
                            Display.getDefault().asyncExec(new Runnable() {

                                @Override
                                public void run() {
                                    syncDialog.setState(State.PUSH);
                                }
                            });
                        } else if (!cancelled && branchStatus.getCommitsAhead() == 0 && branchStatus.getCommitsBehind() == 0) {
                            done = true;
                        }
                        if (done) {
                            IBranchService branchService = (IBranchService)PlatformUI.getWorkbench().getService(IBranchService.class);
                            if (branchService != null) {
                                branchService.updateBranchStatus(null);
                            }
                            Display.getDefault().asyncExec(new Runnable() {

                                @Override
                                public void run() {
                                    syncDialog.setMessage("Your branch is synchronized.");
                                    syncDialog.getButton(IDialogConstants.CANCEL_ID).setEnabled(false);
                                    syncDialog.setHardBaseRevision(null);
                                    syncDialog.setSoftBaseRevision(null);
                                    resolveLibraries();

                                    // Refresh workspace.
                                    // NOTE: This will popup a new dialog. We should use this.indicator
                                    // using IProgressMonitor but this is not how this class is written
                                    // The class should be rewritten soon anyway!
                                    try {
                                        IProgressService service = PlatformUI.getWorkbench().getProgressService();
                                        service.runInUI(service, new IRunnableWithProgress() {
                                            @Override
                                            public void run(IProgressMonitor monitor)
                                                    throws InvocationTargetException,
                                                    InterruptedException {
                                                try {
                                                    ResourcesPlugin.getWorkspace().getRoot().refreshLocal(IResource.DEPTH_INFINITE, monitor);
                                                } catch (CoreException e) {
                                                    Activator.logException(e);
                                                }
                                            }}, null);
                                    } catch (Exception e) {
                                        Activator.logException(e);
                                    }
                                }
                            });
                        }
                        if (!cancelled) {
                            return org.eclipse.core.runtime.Status.OK_STATUS;
                        } else {
                            return org.eclipse.core.runtime.Status.CANCEL_STATUS;
                        }
                    } catch (RepositoryException e) {
                        final String errorMessage = e.getMessage();
                        Display.getDefault().asyncExec(new Runnable() {

                            @Override
                            public void run() {
                                syncDialog.setErrorMessage(errorMessage);
                                indicator.setVisible(false);
                            }
                        });
                    }
                    return org.eclipse.core.runtime.Status.CANCEL_STATUS;
                }
            };
            job.setSystem(true);
            job.schedule();
        }

    }

    private class ResolveLibrariesPage extends ProgressDialogPage {

        public ResolveLibrariesPage(State state, final SyncDialog syncDialog) {
            super(state, syncDialog);
        }

        @Override
        public void onShow() {
            final ProgressIndicator indicator = this.indicator;
            indicator.beginAnimatedTask();
            syncDialog.getButton(IDialogConstants.CANCEL_ID).setEnabled(false);

            IProject project = EditorUtil.getProject();
            IPreferenceStore store = Activator.getDefault().getPreferenceStore();
            final String email = store.getString(PreferenceConstants.P_EMAIL);
            final String authToken = store.getString(PreferenceConstants.P_AUTH_TOKEN);
            final IFolder contentRoot = EditorUtil.getContentRoot(project);

            Job job = new Job("Resolve Libraries") {
                protected IStatus run(IProgressMonitor monitor) {
                    IStatus ret = org.eclipse.core.runtime.Status.OK_STATUS;
                    try {
                        BobUtil.resolveLibs(contentRoot, email, authToken, monitor);
                    } catch (OperationCanceledException e) {
                        // Normal, pass through
                    } catch (Throwable e) {
                        final String errorMessage = e.getMessage();
                        Display.getDefault().asyncExec(new Runnable() {
                            @Override
                            public void run() {
                                syncDialog.setErrorMessage(errorMessage);
                            }
                        });
                        ret = new org.eclipse.core.runtime.Status(IStatus.ERROR, "com.dynamo.cr", errorMessage);
                    } finally {
                        final boolean ok = org.eclipse.core.runtime.Status.OK_STATUS == ret;
                        Display.getDefault().asyncExec(new Runnable() {
                            @Override
                            public void run() {
                                indicator.setVisible(false);
                                String imageId = Activator.DONE_LARGE_IMAGE_ID;
                                String msg = "Library dependencies updated.";
                                if (!ok) {
                                    imageId = Activator.ERROR_LARGE_IMAGE_ID;
                                    msg = "Error resolving libraries";
                                }
                                syncDialog.setTitleImage(Activator.getDefault().getImageRegistry().get(imageId));
                                syncDialog.setMessage(msg);
                                setPageComplete(true);
                            }
                        });
                    }
                    return ret;
                }
            };
            job.setSystem(true);
            job.schedule();
        }
    }

    private class PushDialogPage extends ProgressDialogPage {

        public PushDialogPage(State state, final SyncDialog syncDialog) {
            super(state, syncDialog);
        }

        @Override
        public void onShow() {
            this.indicator.beginAnimatedTask();
            syncDialog.getButton(IDialogConstants.CANCEL_ID).setEnabled(false);
            Job job = new Job("Push To Master") {
                protected IStatus run(IProgressMonitor monitor) {
                    try {
                        IBranchClient branchClient = Activator.getDefault().getBranchClient();
                        branchClient.publish();
                        IBranchService branchService = (IBranchService)PlatformUI.getWorkbench().getService(IBranchService.class);
                        if (branchService != null) {
                            branchService.updateBranchStatus(null);
                        }
                        Display.getDefault().asyncExec(new Runnable() {

                            @Override
                            public void run() {
                                syncDialog.setMessage("Your branch is synchronized.");
                                syncDialog.setHardBaseRevision(null);
                                syncDialog.setSoftBaseRevision(null);
                                resolveLibraries();
                            }
                        });
                        return org.eclipse.core.runtime.Status.OK_STATUS;
                    } catch (RepositoryException e) {
                        final String errorMessage = e.getMessage();
                        Display.getDefault().asyncExec(new Runnable() {

                            @Override
                            public void run() {
                                syncDialog.setErrorMessage(errorMessage);
                                syncDialog.getButton(IDialogConstants.CANCEL_ID).setEnabled(true);
                                indicator.setVisible(false);
                            }
                        });
                    }
                    return org.eclipse.core.runtime.Status.CANCEL_STATUS;
                }
            };
            job.setSystem(true);
            job.schedule();
        }

    }

    private class CommitDialogPage extends SyncDialogPage {

        private BranchStatusTableViewer tableViewer;
        private Text messageText;
        private boolean cancelled;

        public CommitDialogPage(State state, SyncDialog syncDialog) {
            super(state, syncDialog);
            this.cancelled = false;
        }

        @Override
        protected void createContents(final Composite parent) {

            SashForm sash = new SashForm(parent, SWT.VERTICAL);
            sash.setLayoutData(new GridData(GridData.FILL_BOTH));

            Group filesGroup = new Group(sash, SWT.V_SCROLL | SWT.H_SCROLL | SWT.BORDER);
            filesGroup.setText("Files");
            filesGroup.setLayout(new GridLayout());
            tableViewer = new BranchStatusTableViewer(filesGroup, true);
            tableViewer.getControl().setLayoutData(new GridData(GridData.FILL_BOTH));

            Group messageGroup = new Group(sash, SWT.V_SCROLL | SWT.H_SCROLL | SWT.BORDER);
            messageGroup.setText("Message");
            messageGroup.setLayout(new GridLayout());
            messageText = new Text(messageGroup, SWT.MULTI | SWT.BORDER | SWT.WRAP);
            GridData messageData = new GridData(GridData.FILL_BOTH);
            messageData.widthHint = 300;
            messageData.heightHint = 200;
            messageText.setLayoutData(messageData);
            messageText.addModifyListener(new ModifyListener() {

                @Override
                public void modifyText(ModifyEvent e) {
                    if (isControlCreated()) {
                        setPageComplete(!messageText.getText().trim().isEmpty());
                    }
                }
            });
            messageText.addTraverseListener(new TraverseListener() {

                @Override
                public void keyTraversed(TraverseEvent e) {
                    if (e.detail == SWT.TRAVERSE_TAB_NEXT || e.detail == SWT.TRAVERSE_TAB_PREVIOUS) {
                        e.doit = true;
                    }
                }
            });
        }

        @Override
        public void onShow() {
            BranchStatus status = this.syncDialog.getBranchStatus();

            java.util.List<ResourceStatus> resources = new java.util.ArrayList<ResourceStatus>(status.getFileStatusCount());
            for (Status s : status.getFileStatusList()) {
                resources.add(new ResourceStatus(s));
            }

            tableViewer.setInput(resources);
            messageText.setEnabled(true);
            messageText.setVisible(true);
            setPageComplete(false);
            Display.getDefault().asyncExec(new Runnable() {

                @Override
                public void run() {
                    messageText.setFocus();
                }
            });
        }

        @Override
        public boolean onOk() {
            final IBranchClient branchClient = Activator.getDefault().getBranchClient();
            Runnable runnable = new Runnable() {

                @Override
                public void run() {
                    try {
                        if (!cancelled) {
                            Log log = branchClient.log(1);
                            syncDialog.setSoftBaseRevision(log.getCommits(0).getId());
                        }
                        if (!cancelled) {
                            CommitDesc commit = branchClient.commit(messageText.getText());
                            syncDialog.setHardBaseRevision(commit.getId());
                        }
                        if (!cancelled) {
                            final BranchStatus status = syncDialog.updateBranchStatus();
                            Display.getDefault().asyncExec(new Runnable() {

                                @Override
                                public void run() {
                                    if (status.getCommitsBehind() > 0) {
                                        syncDialog.setState(State.PULL);
                                    } else {
                                        syncDialog.setState(State.PUSH);
                                    }
                                }
                            });
                        }
                    } catch (RepositoryException e) {
                        syncDialog.setErrorMessage(e.getMessage());
                    }
                }
            };
            BusyIndicator.showWhile(Display.getDefault(), runnable);
            return false;
        }

        @Override
        public boolean onCancel() {
            this.cancelled = true;
            return true;
        }
    }

    private class ResolveDialogPage extends SyncDialogPage implements IResolveListener {

        private static final String RESOLVE_COMMAND_ID = "com.dynamo.cr.editor.commands.resolve";
        private static final String RESOLVE_COMMAND_TYPE_PARAMETER_ID = "com.dynamo.cr.editor.commands.resolve.type";
        private static final String DIFF_CONFLICT_COMMAND_ID = "com.dynamo.cr.editor.commands.diffConflict";

        private TableViewer conflictsViewer;
        private ConflictedResourceStatus[] conflicts;
        private Button yoursButton;
        private Button theirsButton;
        private boolean cancelled;

        public ResolveDialogPage(State state, SyncDialog syncDialog) {
            super(state, syncDialog);
            cancelled = false;
        }

        @Override
        protected void createContents(Composite parent) {
            Group filesGroup = new Group(parent, SWT.NONE);
            filesGroup.setText("Files");
            filesGroup.setLayoutData(new GridData(GridData.FILL_BOTH));
            filesGroup.setLayout(new GridLayout(2, false));

            this.conflictsViewer = new TableViewer(filesGroup, SWT.H_SCROLL | SWT.V_SCROLL | SWT.FULL_SELECTION | SWT.BORDER | SWT.VIRTUAL);
            this.conflictsViewer.getControl().setLayoutData(new GridData(GridData.FILL_BOTH));
            this.conflictsViewer.setContentProvider(new ArrayContentProvider());
            this.conflictsViewer.addDoubleClickListener(new IDoubleClickListener() {

                @Override
                public void doubleClick(DoubleClickEvent event) {
                    if (!conflictsViewer.getSelection().isEmpty()) {
                        IHandlerService handlerService = (IHandlerService)PlatformUI.getWorkbench().getService(IHandlerService.class);
                        Event e = new Event();
                        e.widget = conflictsViewer.getTable();
                        try {
                            handlerService.executeCommand(DIFF_CONFLICT_COMMAND_ID, e);
                        } catch (Exception e1) {
                            Activator.logException(e1);
                        }
                    }
                }
            });
            this.conflictsViewer.getControl().addKeyListener(new KeyListener() {

                @Override
                public void keyReleased(KeyEvent e) {
                    if (e.keyCode == '\r' && !conflictsViewer.getSelection().isEmpty()) {
                        IHandlerService handlerService = (IHandlerService)PlatformUI.getWorkbench().getService(IHandlerService.class);
                        Event event = new Event();
                        event.widget = conflictsViewer.getTable();
                        try {
                            handlerService.executeCommand(DIFF_CONFLICT_COMMAND_ID, event);
                        } catch (Exception e1) {
                            Activator.logException(e1);
                        }
                    }
                }

                @Override
                public void keyPressed(KeyEvent e) {
                    // TODO Auto-generated method stub

                }
            });

            TableViewerColumn resolveColumn = new TableViewerColumn(this.conflictsViewer, SWT.NONE, 0);
            resolveColumn.getColumn().setWidth(22);
            resolveColumn.setLabelProvider(new ColumnLabelProvider() {
                @Override
                public String getText(Object element) {
                    return "";
                }

                @Override
                public Image getImage(Object element) {
                    ConflictedResourceStatus conflict = (ConflictedResourceStatus)element;
                    ImageRegistry reg = Activator.getDefault().getImageRegistry();
                    switch (conflict.getResolve()) {
                    case UNRESOLVED:
                        return reg.get(Activator.UNRESOLVED_IMAGE_ID);
                    case YOURS:
                        return reg.get(Activator.YOURS_IMAGE_ID);
                    case THEIRS:
                        return reg.get(Activator.THEIRS_IMAGE_ID);
                    default:
                        return null;
                    }
                }

            });

            TableViewerColumn resourceColumn = new TableViewerColumn(this.conflictsViewer, SWT.NONE, 1);
            resourceColumn.getColumn().setWidth(500);
            resourceColumn.setLabelProvider(new ColumnLabelProvider() {
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
                            return Activator.getDefault().getIconByExtension(status.getName());
                        }
                    }
                    return super.getImage(element);
                }

            });

            Composite buttonsComposite = new Composite(filesGroup, SWT.NONE);
            buttonsComposite.setLayoutData(new GridData(GridData.HORIZONTAL_ALIGN_END));
            buttonsComposite.setLayout(new FillLayout(SWT.VERTICAL));
            yoursButton = new Button(buttonsComposite, SWT.RADIO);
            yoursButton.setText("&Yours");
            yoursButton.setImage(Activator.getDefault().getImageRegistry().get(Activator.YOURS_IMAGE_ID));
            yoursButton.setEnabled(false);
            yoursButton.setData(this.conflictsViewer.getTable());
            yoursButton.addSelectionListener(new SelectionListener() {

                @Override
                public void widgetSelected(SelectionEvent e) {
                    IHandlerService handlerService = (IHandlerService)PlatformUI.getWorkbench().getService(IHandlerService.class);
                    Event event = new Event();
                    event.widget = conflictsViewer.getControl();
                    try {
                        Command command = ((ICommandService)PlatformUI.getWorkbench().getService(ICommandService.class)).getCommand(RESOLVE_COMMAND_ID);
                        IParameter typeParameter = command.getParameter(RESOLVE_COMMAND_TYPE_PARAMETER_ID);
                        Parameterization[] parameterizations = { new Parameterization(typeParameter, "yours") };
                        handlerService.executeCommand(new ParameterizedCommand(command, parameterizations), event);
                    } catch (Exception e1) {
                        Activator.logException(e1);
                    }
                }

                @Override
                public void widgetDefaultSelected(SelectionEvent e) {
                    widgetSelected(e);
                }
            });
            theirsButton = new Button(buttonsComposite, SWT.RADIO);
            theirsButton.setText("&Theirs");
            theirsButton.setImage(Activator.getDefault().getImageRegistry().get(Activator.THEIRS_IMAGE_ID));
            theirsButton.setEnabled(false);
            theirsButton.setData(this.conflictsViewer.getTable());
            theirsButton.addSelectionListener(new SelectionListener() {

                @Override
                public void widgetSelected(SelectionEvent e) {
                    IHandlerService handlerService = (IHandlerService)PlatformUI.getWorkbench().getService(IHandlerService.class);
                    Event event = new Event();
                    event.widget = conflictsViewer.getControl();
                    try {
                        Command command = ((ICommandService)PlatformUI.getWorkbench().getService(ICommandService.class)).getCommand(RESOLVE_COMMAND_ID);
                        IParameter typeParameter = command.getParameter(RESOLVE_COMMAND_TYPE_PARAMETER_ID);
                        Parameterization[] parameterizations = { new Parameterization(typeParameter, "theirs") };
                        handlerService.executeCommand(new ParameterizedCommand(command, parameterizations), event);
                    } catch (Exception e1) {
                        Activator.logException(e1);
                    }
                }

                @Override
                public void widgetDefaultSelected(SelectionEvent e) {
                    widgetSelected(e);
                }
            });

            this.conflictsViewer.addSelectionChangedListener(new ISelectionChangedListener() {

                @Override
                public void selectionChanged(SelectionChangedEvent event) {
                    ConflictedResourceStatus conflict = null;
                    if (!event.getSelection().isEmpty()) {
                        conflict = (ConflictedResourceStatus)((IStructuredSelection)event.getSelection()).getFirstElement();
                    }
                    updateUI(conflict);
                }
            });
        }

        @Override
        public void onShow() {
            BranchStatus branchStatus = this.syncDialog.getBranchStatus();
            java.util.List<ConflictedResourceStatus> conflictList = new java.util.ArrayList<ConflictedResourceStatus>(branchStatus.getFileStatusCount());
            for (Status status : branchStatus.getFileStatusList()) {
                // NOTE: This is broken and doens't handle cases with deleted files
                // See case 1906 and case 1348
                if (status.getIndexStatus().equals("U")
                 || (status.getIndexStatus().equals("A") && status.getWorkingTreeStatus().equals("A"))) {
                    ConflictedResourceStatus conflict = new ConflictedResourceStatus(status);
                    conflictList.add(conflict);
                    conflict.addResolveListener(this);
                }
            }
            this.conflicts = new ConflictedResourceStatus[conflictList.size()];
            conflictList.toArray(this.conflicts);
            this.conflictsViewer.setInput(this.conflicts);
            this.yoursButton.setSelection(false);
            this.theirsButton.setSelection(false);
            setPageComplete(false);
            Display.getDefault().asyncExec(new Runnable() {

                @Override
                public void run() {
                    conflictsViewer.getControl().setFocus();
                }
            });
        }

        public boolean onOk() {
            final IBranchClient branchClient = Activator.getDefault().getBranchClient();
            final ConflictedResourceStatus[] conflicts = (ConflictedResourceStatus[])this.conflictsViewer.getInput();
            Runnable runnable = new Runnable() {

                @Override
                public void run() {
                    try {
                        if (syncDialog.getHardBaseRevision() == null) {
                            Log log = branchClient.log(1);
                            syncDialog.setHardBaseRevision(log.getCommits(0).getId());
                        }
                        StringBuffer message = new StringBuffer();
                        message.append("Resolved conflicts.").append("\n");
                        for (ConflictedResourceStatus conflict : conflicts) {
                            if (cancelled) {
                                return;
                            }
                            String path = conflict.getStatus().getName();
                            String resolve = conflict.getResolve().toString().toLowerCase();
                            branchClient.resolve(path, resolve);
                            message.append("\n").append(resolve).append(" - ").append(path);
                        }
                        message.append("\n");
                        if (!cancelled) {
                            branchClient.commitMerge(message.toString());
                        }
                        if (!cancelled) {
                            final BranchStatus status = branchClient.getBranchStatus(true);
                            Display.getDefault().asyncExec(new Runnable() {

                                @Override
                                public void run() {
                                    if (status.getCommitsBehind() > 0) {
                                        syncDialog.setState(State.PULL);
                                    } else {
                                        syncDialog.setState(State.PUSH);
                                    }
                                }
                            });
                        }
                    } catch (RepositoryException e) {
                        syncDialog.setErrorMessage(e.getMessage());
                    }
                }
            };
            BusyIndicator.showWhile(Display.getDefault(), runnable);
            return false;
        }

        public boolean onCancel() {
            this.cancelled = true;
            return true;
        }

        @Override
        public void handleResolve(ConflictedResourceStatus conflict) {
            updateUI(conflict);
        }

        private void updateUI(ConflictedResourceStatus conflict) {
            this.conflictsViewer.update(conflict, null);
            if (!this.conflictsViewer.getSelection().isEmpty()) {
                if (((IStructuredSelection)this.conflictsViewer.getSelection()).getFirstElement() == conflict) {
                    yoursButton.setEnabled(true);
                    theirsButton.setEnabled(true);
                    switch (conflict.getResolve()) {
                    case YOURS:
                        yoursButton.setSelection(true);
                        theirsButton.setSelection(false);
                        break;
                    case THEIRS:
                        theirsButton.setSelection(true);
                        yoursButton.setSelection(false);
                    default:
                        break;
                    }
                }
            } else {
                yoursButton.setEnabled(false);
                theirsButton.setEnabled(false);
            }
            ConflictedResourceStatus[] conflicts = (ConflictedResourceStatus[])this.conflictsViewer.getInput();
            boolean isComplete = true;
            for (ConflictedResourceStatus c : conflicts) {
                if (c.getResolve() == Resolve.UNRESOLVED) {
                    isComplete = false;
                }
            }
            setPageComplete(isComplete);
        }
    }

}
