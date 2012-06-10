package com.dynamo.cr.target.core;

import java.io.IOException;

public interface IURLFetcher {

    public String fetch(String url) throws IOException;
}
