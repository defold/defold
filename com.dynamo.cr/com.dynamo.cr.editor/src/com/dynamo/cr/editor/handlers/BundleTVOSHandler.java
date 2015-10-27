package com.dynamo.cr.editor.handlers;

import java.util.Map;

import org.eclipse.jface.dialogs.Dialog;

import com.dynamo.cr.target.bundle.BundleTVOSDialog;
import com.dynamo.cr.target.bundle.BundleTVOSPresenter;
import com.dynamo.cr.target.bundle.IBundleTVOSView;
import com.dynamo.cr.target.sign.IIdentityLister;
import com.dynamo.cr.target.sign.IdentityLister;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class BundleTVOSHandler extends AbstractBundleHandler {

    private BundleTVOSDialog view;
    private IdentityLister identityLister;
    private BundleTVOSPresenter presenter;

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(IBundleTVOSView.class).toInstance(view);
            bind(IIdentityLister.class).toInstance(identityLister);
        }
    }

    @Override
    protected boolean openBundleDialog() {
        view = new BundleTVOSDialog(shell);
        identityLister = new IdentityLister();
        Module module = new Module();
        Injector injector = Guice.createInjector(module);
        presenter = injector.getInstance(BundleTVOSPresenter.class);
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
        String profile = presenter.getProvisioningProfile();
        String identity = presenter.getIdentity();
        options.put("mobileprovisioning", profile);
        options.put("identity", identity);
        options.put("platform", "arm64-tvos"); // TODO: IS THIS VALID?? WAS armv7-darwin
        if(!presenter.isReleaseMode()) {
            options.put("debug", "true");
        }
    }

}
