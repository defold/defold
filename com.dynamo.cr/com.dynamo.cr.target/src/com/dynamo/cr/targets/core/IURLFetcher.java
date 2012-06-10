package com.dynamo.cr.targets.core;

import java.io.IOException;

public interface IURLFetcher {

    public String fetch(String url) throws IOException;
}
