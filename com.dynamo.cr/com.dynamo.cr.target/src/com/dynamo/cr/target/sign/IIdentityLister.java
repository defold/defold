package com.dynamo.cr.target.sign;

import java.io.IOException;

public interface IIdentityLister {
    public String[] listIdentities() throws IOException;
}
