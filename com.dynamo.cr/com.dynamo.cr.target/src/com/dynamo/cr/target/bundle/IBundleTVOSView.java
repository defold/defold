package com.dynamo.cr.target.bundle;

import com.dynamo.cr.target.bundle.BundleTVOSDialog.IPresenter;

public interface IBundleTVOSView {

    void setErrorMessage(String msg);
    void setWarningMessage(String msg);
    void setMessage(String msg);
    void setEnabled(boolean enabled);
    void setIdentities(String[] identities);
    void setPresenter(IPresenter presenter);

}
