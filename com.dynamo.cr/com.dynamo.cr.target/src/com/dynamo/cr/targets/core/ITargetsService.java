package com.dynamo.cr.targets.core;

import java.net.URL;

public interface ITargetsService {
    public static final String LOCAL_TARGET_ID = "local";

    public void addTargetsListener(ITargetsListener listener);

    public void removeTargetsListener(ITargetsListener listener);

    public ITarget[] getTargets();

    public abstract void stop();

    public void setSearchInternal(int searchInterval);

    public void launch(String customApplication, String location, boolean runInDebugger, boolean autoRunDebugger,
            String socksProxy, int socksProxyPort, URL serverUrl);

    public ITarget getSelectedTarget();
}
