package com.dynamo.cr.editor.handlers;

import java.util.Map;

import org.apache.commons.io.FilenameUtils;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.preference.IPreferenceStore;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.preferences.PreferenceConstants;
import com.dynamo.cr.target.bundle.BundleiOSDialog;
import com.dynamo.cr.target.bundle.BundleiOSPresenter;
import com.dynamo.cr.target.bundle.IBundleiOSView;
import com.dynamo.cr.target.sign.IIdentityLister;
import com.dynamo.cr.target.sign.IdentityLister;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class BundleiOSHandler extends AbstractBundleHandler {

    private BundleiOSDialog view;
    private IdentityLister identityLister;
    private BundleiOSPresenter presenter;
    private static String PLATFORM_STRING = "armv7-darwin";

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(IBundleiOSView.class).toInstance(view);
            bind(IIdentityLister.class).toInstance(identityLister);
        }
    }

    @Override
    protected boolean openBundleDialog() {
        view = new BundleiOSDialog(shell);
        identityLister = new IdentityLister();
        Module module = new Module();
        Injector injector = Guice.createInjector(module);
        presenter = injector.getInstance(BundleiOSPresenter.class);
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
        String profile = presenter.getProvisioningProfile();
        String identity = presenter.getIdentity();
        options.put("mobileprovisioning", profile);
        options.put("identity", identity);
        options.put("platform", PLATFORM_STRING);
        options.put("build-server", store.getString(PreferenceConstants.P_NATIVE_EXT_SERVER_URI));

        EditorCorePlugin corePlugin = EditorCorePlugin.getDefault();
        String sdkVersion = corePlugin.getSha1();
        if (sdkVersion == "NO SHA1") {
            sdkVersion = "";
        }
        options.put("defoldsdk", sdkVersion);

        if(!presenter.isReleaseMode()) {
            options.put("debug", "true");
        }
        if(presenter.isSimulatorBinary()) {
            options.put("platform", "x86_64-ios");
        }
        if(presenter.shouldGenerateReport()) {
            options.put("build-report-html", FilenameUtils.concat(outputDirectory, "report.html"));
        }

        if (presenter.shouldPublishLiveUpdate()) {
            options.put("liveupdate", "true");
        }
    }

    @Override
    protected String getOutputPlatformDir() {
        return PLATFORM_STRING;
    }

}
