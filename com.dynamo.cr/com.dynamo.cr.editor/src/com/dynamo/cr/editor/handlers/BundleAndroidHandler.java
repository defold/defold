package com.dynamo.cr.editor.handlers;

import java.util.Map;

import org.eclipse.jface.dialogs.Dialog;

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
    protected void setProjectOptions(Map<String, String> options) {
        String certificate = presenter.getCertificate();
        String key = presenter.getKey();
        options.put("certificate", certificate);
        options.put("private-key", key);
        options.put("platform", "armv7-android");
        if(!presenter.isReleaseMode()) {
            options.put("debug", "true");
        }
    }

}
