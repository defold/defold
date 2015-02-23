package com.dynamo.cr.editor.handlers;

import java.util.Map;

import org.eclipse.jface.dialogs.Dialog;

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
        String profile = presenter.getProvisioningProfile();
        String identity = presenter.getIdentity();
        options.put("mobileprovisioning", profile);
        options.put("identity", identity);
        options.put("platform", "armv7-darwin");
        if(!presenter.isReleaseMode()) {
            options.put("debug", "true");
        }
    }

}
