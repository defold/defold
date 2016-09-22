package com.dynamo.cr.target.bundle;

import java.io.File;

import javax.inject.Inject;

public class BundleAndroidPresenter implements BundleAndroidDialog.IPresenter {

    private IBundleAndroidView view;
    private String certificate = "";
    private String key = "";
    private boolean releaseMode;
    private boolean generateReport;

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

        final boolean certProvided = !certificate.isEmpty();
        final boolean keyProvided = !key.isEmpty();
        if (certProvided && !new File(certificate).isFile()) {
            view.setErrorMessage("Certificate not found");
            return;
        }
        if (keyProvided && !new File(key).isFile()) {
            view.setErrorMessage("Key not found");
            return;
        }
        // enable view only if both key and cert are provided or neither of them
        if((!certProvided && keyProvided) || (certProvided && !keyProvided)) {
        	return;
        }

        // Only warnings after this point
        view.setEnabled(true);
    }

    @Override
    public void setKey(String identity) {
        this.setKey(identity, true);
    }

    @Override
    public void setKey(String identity, boolean validate) {
        this.key = identity;
        if (validate) {
            validate();
        }
    }

    public String getKey() {
        return key;
    }

    @Override
    public void setCertificate(String certificate) {
        this.setCertificate(certificate, true);
    }

    @Override
    public void setCertificate(String certificate, boolean validate) {
        this.certificate = certificate;
        if (validate) {
            validate();
        }
    }

    public String getCertificate() {
        return certificate;
    }

    public boolean isReleaseMode() {
    	return releaseMode;
    }

    @Override
    public void releaseModeSelected(boolean selection) {
        this.releaseModeSelected(selection, true);
    }

    public void releaseModeSelected(boolean selection, boolean validate) {
        this.releaseMode = selection;
        if (validate) {
            validate();
        }
    }

    public boolean shouldGenerateReport() {
        return generateReport;
    }

    @Override
    public void generateReportSelected(boolean selection) {
        this.generateReportSelected(selection, true);
    }

    public void generateReportSelected(boolean selection, boolean validate) {
        this.generateReport = selection;
        if (validate) {
            validate();
        }
    }

}
