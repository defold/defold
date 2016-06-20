package com.dynamo.cr.target.bundle;
import javax.inject.Inject;

public class BundleGenericPresenter implements BundleGenericDialog.IPresenter {

    private IBundleGenericView view;
    private boolean releaseMode;
    private boolean generateReport;

    @Inject
    public BundleGenericPresenter(IBundleGenericView view) {
        this.view = view;
    }

    @Override
    public void start() {
        try {
            validate();
        } catch (Exception e) {
            view.setErrorMessage(e.getMessage());
        }
    }

    private void validate() {
        // Set defaults
        view.setEnabled(false);
        view.setMessage(Messages.BundleGenericPresenter_DIALOG_MESSAGE);

        // Only warnings after this point
        view.setEnabled(true);
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
