package com.dynamo.cr.target.sign;

import com.dynamo.cr.target.sign.SignDialog.IPresenter;

public interface ISignView {

    void setErrorMessage(String msg);
    void setMessage(String msg);
    void setEnabled(boolean enabled);
    void setIdentities(String[] identities);
    void setPresenter(IPresenter presenter);

}
