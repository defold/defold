package com.dynamo.cr.target.bundle;

import com.dynamo.cr.target.bundle.BundleiOSDialog.IPresenter;

public interface IBundleiOSView {

    void setErrorMessage(String msg);
    void setWarningMessage(String msg);
    void setMessage(String msg);
    void setEnabled(boolean enabled);
    void setIdentities(String[] identities);
    void setPresenter(IPresenter presenter);
    void setVersion(String version);

}
