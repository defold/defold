package com.dynamo.cr.target.core;


public interface ITargetService {
    public static final String LOCAL_TARGET_ID = "local";

    public void addTargetsListener(ITargetListener listener);

    public void removeTargetsListener(ITargetListener listener);

    public ITarget[] getTargets();

    public abstract void stop();

    public void setSearchInternal(int searchInterval);

    public void launch(String customApplication, String location, boolean runInDebugger, boolean autoRunDebugger,
                       String socksProxy, int socksProxyPort, int httpServerPort, boolean quitOnEsc);

    public ITarget getSelectedTarget();

    public void search();
}
