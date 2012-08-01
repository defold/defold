package com.dynamo.cr.editor.ui;

import javax.inject.Singleton;

import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.application.IWorkbenchWindowConfigurer;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.editor.ui.internal.EditorWindow;
import com.dynamo.cr.editor.ui.tip.ITipManager;
import com.dynamo.cr.editor.ui.tip.TipManager;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Provides;

/**
 * The activator class controls the plug-in life cycle
 */
public class EditorUIPlugin extends AbstractUIPlugin {

    // The plug-in ID
    public static final String PLUGIN_ID = "com.dynamo.cr.editor.ui"; //$NON-NLS-1$

    // The shared instance
    private static EditorUIPlugin plugin;

    private IWorkbenchWindowConfigurer workbenchWindowConfigurer;

    class Module extends AbstractModule {

        @Override
        protected void configure() {
            bind(ITipManager.class).to(TipManager.class).in(Singleton.class);
            bind(IEditorWindow.class).to(EditorWindow.class).in(Singleton.class);
        }

        @Provides
        IWorkbenchWindowConfigurer provideIWorkbenchWindowConfigurer() {
            return workbenchWindowConfigurer;
        }
    }

    private Module module;
    private Injector injector;

    /**
     * The constructor
     */
    public EditorUIPlugin() {
        module = new Module();
        injector = Guice.createInjector(module);
    }

    /*
     * (non-Javadoc)
     * @see org.eclipse.ui.plugin.AbstractUIPlugin#start(org.osgi.framework.BundleContext)
     */
    @Override
    public void start(BundleContext context) throws Exception {
        super.start(context);
        plugin = this;
    }

    /*
     * (non-Javadoc)
     * @see org.eclipse.ui.plugin.AbstractUIPlugin#stop(org.osgi.framework.BundleContext)
     */
    @Override
    public void stop(BundleContext context) throws Exception {
        plugin = null;
        super.stop(context);
    }

    /**
     * Returns the shared instance
     *
     * @return the shared instance
     */
    public static EditorUIPlugin getDefault() {
        return plugin;
    }

    public IEditorWindow createWindow(Shell shell, IWorkbenchWindowConfigurer windowConfigurer) {
        // NOTE: This is a hack for injection but we don't have IWorkbenchWindowConfigurer when the plugin
        // is started
        workbenchWindowConfigurer = windowConfigurer;
        IEditorWindow editorWindow = injector.getInstance(IEditorWindow.class);
        return editorWindow;
    }

    public Injector getInjector() {
        return injector;
    }
}
