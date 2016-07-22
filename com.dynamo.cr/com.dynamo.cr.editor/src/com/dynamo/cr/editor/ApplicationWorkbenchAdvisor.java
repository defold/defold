package com.dynamo.cr.editor;

import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.preferences.InstanceScope;
import org.eclipse.jface.preference.IPreferenceNode;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.jface.preference.PreferenceManager;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.application.IWorkbenchConfigurer;
import org.eclipse.ui.application.IWorkbenchWindowConfigurer;
import org.eclipse.ui.application.WorkbenchAdvisor;
import org.eclipse.ui.application.WorkbenchWindowAdvisor;
import org.eclipse.ui.handlers.IHandlerService;
import org.eclipse.ui.internal.ide.IDEInternalPreferences;
import org.eclipse.ui.internal.ide.IDEWorkbenchPlugin;
import org.eclipse.ui.navigator.resources.ProjectExplorer;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.preferences.PreferenceConstants;

public class ApplicationWorkbenchAdvisor extends WorkbenchAdvisor {

    private static final String PERSPECTIVE_ID = "com.dynamo.cr.editor.perspective"; //$NON-NLS-1$

    public WorkbenchWindowAdvisor createWorkbenchWindowAdvisor(IWorkbenchWindowConfigurer configurer) {
        return new ApplicationWorkbenchWindowAdvisor(configurer);
    }

    public String getInitialWindowPerspectiveId() {
        return PERSPECTIVE_ID;
    }

    @Override
    public void initialize(IWorkbenchConfigurer configurer) {
        super.initialize(configurer);
        org.eclipse.ui.ide.IDE.registerAdapters();
    }

    protected void dumpPreferencesNodes(IPreferenceNode[] nodes, int indent) {

        for (IPreferenceNode n : nodes) {
            for (int i = 0; i < indent; ++i) {
                System.out.print(' ');
            }
            System.out.println(n.getId());

            IPreferenceNode[] subs = n.getSubNodes();
            dumpPreferencesNodes(subs, indent + 2);
        }

    }

    @Override
    public boolean preShutdown() {
        boolean ret = super.preShutdown();
        try {
            // Save the full workspace before quit
            // Otherwise eclipse will warn about unsaved changes
            ResourcesPlugin.getWorkspace().save(true, null);
        } catch (final CoreException e) {
            // We ignore this one
        }
        return ret;
    }

    static class AutoRefreshActivator implements Listener {

        boolean autoRefreshOn = false;

        @Override
        public void handleEvent(Event event) {
            // "Auto-refresh with native hooks or polling"
            try {
                Display d = Display.getDefault();
                Shell as = d.getActiveShell();
                if (as != null && autoRefreshOn == false) {
                    autoRefreshOn = true;
                    InstanceScope.INSTANCE.getNode("org.eclipse.core.resources").putBoolean(ResourcesPlugin.PREF_AUTO_REFRESH, true);
                } else if (as == null && autoRefreshOn == true) {
                    autoRefreshOn = false;
                    InstanceScope.INSTANCE.getNode("org.eclipse.core.resources").putBoolean(ResourcesPlugin.PREF_AUTO_REFRESH, false);
                }
            } catch (Throwable e) {
                // NOTE: We catch all exceptions here as it's *dangerous* to
                // throw exceptions from swt filter code
            }
        }
    }

    @Override
    public void postStartup() {
        super.postStartup();

        // Enable/disable automatic refresh based on when editor is active/not active
        // Primary reason is to disable auto-staging of files but probably good in general
        // for performance reasons
        AutoRefreshActivator ra = new AutoRefreshActivator();
        Display.getDefault().addFilter(SWT.Activate, ra);
        Display.getDefault().addFilter(SWT.Deactivate, ra);

        /*
         * TODO: Hack from http://www.eclipse.org/forums/index.php?t=msg&goto=486705&
         */
        IWorkbenchWindow[] workbenchs = PlatformUI.getWorkbench()
                .getWorkbenchWindows();

        ProjectExplorer view = null;
        for (IWorkbenchWindow workbench : workbenchs) {
            for (IWorkbenchPage page : workbench.getPages()) {
                view = (ProjectExplorer) page
                        .findView("org.eclipse.ui.navigator.ProjectExplorer");
                break;
            }
        }

        if (view == null) {
            return;
        }

        view.getCommonViewer().setInput(
                ResourcesPlugin.getWorkspace().getRoot());

        // Always copy files to project when files and dropped. Do not prompt for copy/link options (dialog)
        // NOTE: Internal stuff. There is perhaps a better way?
        // We could disable linked resources in project nature but currently we link the file-store and not possible
        // With local pipeline we could set <options allowLinking="false"/> to project nature instead
        IPreferenceStore ideWorkbenchStore = IDEWorkbenchPlugin.getDefault().getPreferenceStore();
        ideWorkbenchStore.setValue(IDEInternalPreferences.IMPORT_FILES_AND_FOLDERS_MODE, IDEInternalPreferences.IMPORT_FILES_AND_FOLDERS_MODE_MOVE_COPY);

        // Remove unwanted preferences pages
        // TODO: We should perhaps remove these using activities instead?
        // We currently remove "New Project" etc using this feature
        // See http://stackoverflow.com/questions/1460761/howto-hide-a-preference-page-in-an-eclipse-rcp
        // See post with <activityPatternBinding...
        PreferenceManager pm = PlatformUI.getWorkbench().getPreferenceManager( );
        pm.remove("org.eclipse.team.ui.TeamPreferences");
        pm.remove("org.eclipse.ui.preferencePages.Workbench/org.eclipse.ui.preferencePages.Perspectives");
        pm.remove("org.eclipse.ui.preferencePages.Workbench/org.eclipse.equinox.security.ui.category");
        pm.remove("org.eclipse.ui.preferencePages.Workbench/org.eclipse.ui.net.NetPreferences");
        pm.remove("org.eclipse.ui.preferencePages.Workbench/org.eclipse.ui.preferencePages.ContentTypes");
        pm.remove("org.eclipse.ui.preferencePages.Workbench/org.eclipse.ui.preferencePages.Workspace");
        pm.remove("org.eclipse.ui.preferencePages.Workbench/org.eclipse.compare.internal.ComparePreferencePage");
        pm.remove("org.eclipse.ui.preferencePages.Workbench/org.eclipse.ui.preferencePages.Views/org.eclipse.ui.preferencePages.Decorators");
        pm.remove("org.eclipse.equinox.internal.p2.ui.sdk.ProvisioningPreferencePage");
        pm.remove("org.eclipse.ui.preferencePages.Workbench/org.eclipse.ui.preferencePages.Editors/org.eclipse.ui.preferencePages.GeneralTextEditor/org.eclipse.ui.editors.preferencePages.QuickDiff");
        pm.remove("org.eclipse.ui.preferencePages.Workbench/org.eclipse.ui.preferencePages.Editors/org.eclipse.ui.preferencePages.GeneralTextEditor/org.eclipse.ui.editors.preferencePages.Accessibility");
        pm.remove("org.eclipse.ui.preferencePages.Workbench/org.eclipse.ui.preferencePages.Editors/org.eclipse.ui.preferencePages.GeneralTextEditor/org.eclipse.ui.editors.preferencePages.Spelling");
        pm.remove("org.eclipse.ui.preferencePages.Workbench/org.eclipse.ui.preferencePages.Editors/org.eclipse.ui.preferencePages.GeneralTextEditor/org.eclipse.ui.editors.preferencePages.LinkedModePreferencePage");
        pm.remove("org.eclipse.ui.preferencePages.Workbench/org.eclipse.ui.preferencePages.Editors/org.eclipse.ui.preferencePages.GeneralTextEditor/org.eclipse.ui.editors.preferencePages.HyperlinkDetectorsPreferencePage");

        if (!EditorUtil.isDev()) {
            // Remove debug preferences for end-users
            pm.remove("com.dynamo.rclient.preferences.PreferencePage/com.dynamo.rclient.preferences.DebugPreferencePage");
        }

        // NOTE: Uncomment line below to dump all preference nodes. Use / as separator. See above
        //dumpPreferencesNodes(pm.getRootSubNodes(), 0);

        final IPreferenceStore editorStore = Activator.getDefault().getPreferenceStore();
        if (editorStore.getBoolean(PreferenceConstants.P_SHOW_WELCOME_PAGE)) {
            editorStore.setValue(PreferenceConstants.P_SHOW_WELCOME_PAGE, false);
            Display.getDefault().asyncExec(new Runnable() {
                @Override
                public void run() {
                    IHandlerService handlerService = (IHandlerService) PlatformUI.getWorkbench().getService(IHandlerService.class);
                    try {
                        handlerService.executeCommand("com.dynamo.cr.editor.commands.openWelcome", null);
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }
                }
            });
        }
    }
}
