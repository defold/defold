package com.dynamo.cr.server;

public interface ServerMBean {
    int getETagCacheHits();
    int getETagCacheMisses();
    int getResourceInfoRequests();
    int getResourceDataRequests();
}
