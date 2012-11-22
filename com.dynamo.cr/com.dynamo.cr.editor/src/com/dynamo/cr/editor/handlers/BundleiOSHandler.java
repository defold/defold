package com.dynamo.cr.editor.handlers;

import java.io.IOException;

import org.apache.commons.configuration.ConfigurationException;
import org.eclipse.jface.dialogs.Dialog;

import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.engine.Engine;
import com.dynamo.cr.target.bundle.BundleiOSDialog;
import com.dynamo.cr.target.bundle.BundleiOSPresenter;
import com.dynamo.cr.target.bundle.IBundleiOSView;
import com.dynamo.cr.target.bundle.IOSBundler;
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
    protected void bundleApp(ProjectProperties projectProperties,
            String projectRoot, String contentRoot, String outputDir)
            throws ConfigurationException, IOException {

        String identity = presenter.getIdentity();
        String profile = presenter.getProvisioningProfile();

        String exe = Engine.getDefault().getEnginePath("ios", true);
        IOSBundler bundler = new IOSBundler(identity, profile,  projectProperties, exe, projectRoot, contentRoot, outputDir);
        bundler.bundleApplication();
    }

}
