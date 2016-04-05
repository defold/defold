package com.dynamo.cr.target.bundle;

import com.dynamo.cr.target.bundle.BundleGenericDialog.IPresenter;

public interface IBundleGenericView {

    void setErrorMessage(String msg);
    void setWarningMessage(String msg);
    void setMessage(String msg);
    void setEnabled(boolean enabled);
    void setPresenter(IPresenter presenter);

}
