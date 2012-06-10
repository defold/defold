package com.dynamo.cr.targets.core;

public class Target implements ITarget {

    private String name;
    private String id;
    private String url;

    public Target(String name, String id, String url) {
        this.name = name;
        this.id = id;
        this.url = url;
    }

    @Override
    public String getName() {
        return name;
    }

    @Override
    public String getId() {
        return id;
    }

    @Override
    public String getUrl() {
        return url;
    }

    @Override
    public String toString() {
        return String.format("%s (%s)", id, url);
    }

}
