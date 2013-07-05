package com.dynamo.cr.editor.handlers;

import java.io.IOException;

import org.apache.commons.configuration.ConfigurationException;
import org.eclipse.jface.dialogs.Dialog;

import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.engine.Engine;
import com.dynamo.cr.target.bundle.AndroidBundler;
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
    protected void bundleApp(ProjectProperties projectProperties,
            String projectRoot, String contentRoot, String outputDir)
            throws ConfigurationException, IOException {

        String certificate = presenter.getCertificate();
        String key = presenter.getKey();

        String exe = Engine.getDefault().getEnginePath("android", true);
        AndroidBundler bundler = new AndroidBundler(certificate, key,  projectProperties, exe, projectRoot, contentRoot, outputDir);
        bundler.bundleApplication();
    }

}
