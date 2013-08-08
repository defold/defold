package com.dynamo.cr.target.bundle;

import com.dynamo.cr.target.bundle.BundleAndroidDialog.IPresenter;

public interface IBundleAndroidView {

    void setErrorMessage(String msg);
    void setWarningMessage(String msg);
    void setMessage(String msg);
    void setEnabled(boolean enabled);
    void setPresenter(IPresenter presenter);

}
