package com.dynamo.cr.server;

import com.dynamo.cr.protocol.proto.Protocol.BuildDesc;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc.Activity;

public class RuntimeBuildDesc {

    public RuntimeBuildDesc() {
        id = -1;
        progress = -1;
        workAmount = -1;
        buildCompleted = System.currentTimeMillis();
    }
    public int id;
    public Activity activity;
    public BuildDesc.Result buildResult;
    public int progress;
    public int workAmount;
    public String stdOut = "";
    public String stdErr = "";
    public long buildCompleted;
    public IBuildRunnable buildRunnable;
    public Thread buildThread;
    public String project;
    public String user;
    public String branch;
}
