package com.dynamo.cr.target.bundle;

import java.io.File;

import javax.inject.Inject;

public class BundleAndroidPresenter implements BundleAndroidDialog.IPresenter {

    private IBundleAndroidView view;
    private String certificate = "";
    private String key = "";
    private boolean releaseMode;

    @Inject
    public BundleAndroidPresenter(IBundleAndroidView view) {
        this.view = view;
    }

    @Override
    public void start() {
        validate();
    }

    private void validate() {
        // Set defaults
        view.setEnabled(false);
        view.setMessage(Messages.BundleAndroidPresenter_DIALOG_MESSAGE);
        view.setErrorMessage(null);

        if (releaseMode) {
            if (certificate.length() == 0) {
                return;
            }

            if (key.length() == 0) {
                return;
            }

            if (!new File(certificate).isFile()) {
                view.setErrorMessage("Certificate not found");
                return;
            }
            if (!new File(key).isFile()) {
                view.setErrorMessage("Key not found");
                return;
            }
        }

        // Only warnings after this point
        view.setEnabled(true);
    }

    @Override
    public void setKey(String identity) {
        this.key = identity;
        validate();
    }

    public String getKey() {
        return key;
    }

    @Override
    public void setCertificate(String certificate) {
        this.certificate = certificate;
        validate();
    }

    public String getCertificate() {
        return certificate;
    }

    @Override
    public void releaseModeSelected(boolean selection) {
        this.releaseMode = selection;
        validate();
    }

}
