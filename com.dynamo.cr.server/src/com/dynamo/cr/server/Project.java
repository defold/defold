package com.dynamo.cr.server;

import com.dynamo.cr.proto.Config.ProjectConfiguration;

public class Project {

    private String id;
    private String name;
    private String root;
    public ProjectConfiguration configuration;

    public Project(ProjectConfiguration pc) {
        this.configuration = pc;
        this.id = pc.getId();
        this.name = pc.getName();
        this.root = pc.getRoot();
    }

    public String getId() {
        return id;
    }

    public String getName() {
        return name;
    }

    public String getRoot() {
        return root;
    }

}
