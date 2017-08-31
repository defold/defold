package com.dynamo.cr.editor.handlers;

import java.util.Map;

import org.apache.commons.io.FilenameUtils;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.preference.IPreferenceStore;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.preferences.PreferenceConstants;
import com.dynamo.cr.target.bundle.BundleGenericDialog;
import com.dynamo.cr.target.bundle.BundleGenericPresenter;
import com.dynamo.cr.target.bundle.IBundleGenericView;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

/**
 * Bundle Generic handler
 * TODO: The Generic is located in com.dynamo.cr.editor in order
 * to avoid cyclic dependencies between .editor and .target
 * @author anta
 *
 */
public abstract class BundleGenericHandler extends AbstractBundleHandler {

    protected BundleGenericDialog view;
    protected BundleGenericPresenter presenter;
    String title = "";

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(IBundleGenericView.class).toInstance(view);
        }
    }

    protected void setTitle(String title) {
        this.title = title;
    }

    @Override
    protected boolean openBundleDialog() {
        view = new BundleGenericDialog(shell);
        view.setTitle(title);
        Module module = new Module();
        Injector injector = Guice.createInjector(module);
        presenter = injector.getInstance(BundleGenericPresenter.class);
        view.setPresenter(presenter);
        int ret = view.open();
        if (ret == Dialog.OK) {
            return true;
        } else {
            return false;
        }
    }

    @Override
    protected void setProjectOptions(Map<String, String> options) {
        if(!presenter.isReleaseMode()) {
            options.put("debug", "true");
        }
        if(presenter.shouldGenerateReport()) {
            options.put("build-report-html", FilenameUtils.concat(outputDirectory, "report.html"));
        }

        if (presenter.shouldPublishLiveUpdate()) {
            options.put("liveupdate", "true");
        }

        final IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        options.put("build-server", store.getString(PreferenceConstants.P_NATIVE_EXT_SERVER_URI));

        EditorCorePlugin corePlugin = EditorCorePlugin.getDefault();
        String sdkVersion = corePlugin.getSha1();
        if (sdkVersion == "NO SHA1") {
            sdkVersion = "";
        }
        options.put("defoldsdk", sdkVersion);
    }

}
