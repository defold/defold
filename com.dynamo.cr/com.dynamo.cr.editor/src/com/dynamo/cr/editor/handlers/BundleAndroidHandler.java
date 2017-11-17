package com.dynamo.cr.editor.handlers;

import java.util.Map;

import org.apache.commons.io.FilenameUtils;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.preference.IPreferenceStore;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.preferences.PreferenceConstants;
import com.dynamo.cr.target.bundle.BundleAndroidDialog;
import com.dynamo.cr.target.bundle.BundleAndroidPresenter;
import com.dynamo.cr.target.bundle.IBundleAndroidView;
import com.dynamo.cr.target.sign.IIdentityLister;
import com.dynamo.cr.target.sign.IdentityLister;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class BundleAndroidHandler extends AbstractBundleHandler {

    private BundleAndroidDialog view;
    private IdentityLister identityLister;
    private BundleAndroidPresenter presenter;

    private static String PLATFORM_STRING = "armv7-android";

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(IBundleAndroidView.class).toInstance(view);
            bind(IIdentityLister.class).toInstance(identityLister);
        }
    }

    @Override
    protected boolean openBundleDialog() {
        view = new BundleAndroidDialog(shell);
        identityLister = new IdentityLister();
        Module module = new Module();
        Injector injector = Guice.createInjector(module);
        presenter = injector.getInstance(BundleAndroidPresenter.class);
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
        final IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        String certificate = presenter.getCertificate();
        String key = presenter.getKey();
        options.put("certificate", certificate);
        options.put("private-key", key);
        options.put("platform", PLATFORM_STRING);
        if(!presenter.isReleaseMode()) {
            options.put("debug", "true");
        }
        if(presenter.shouldGenerateReport()) {
            options.put("build-report-html", FilenameUtils.concat(outputDirectory, "report.html"));
        }

        if (presenter.shouldPublishLiveUpdate()) {
            options.put("liveupdate", "true");
        }

        options.put("build-server", store.getString(PreferenceConstants.P_NATIVE_EXT_SERVER_URI));

        EditorCorePlugin corePlugin = EditorCorePlugin.getDefault();
        String sdkVersion = corePlugin.getSha1();
        if (sdkVersion == "NO SHA1") {
            sdkVersion = "";
        }
        options.put("defoldsdk", sdkVersion);
    }

    @Override
    protected String getOutputPlatformDir() {
        return PLATFORM_STRING;
    }

}
