package com.dynamo.cr.target.core;

public class Target implements ITarget {

    private String name;
    private String id;
    private String url;
    private int logPort;

    public Target(String name, String id, String url, int logPort) {
        this.name = name;
        this.id = id;
        this.url = url;
        this.logPort = logPort;
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
    public int getLogPort() {
        return logPort;
    }

    @Override
    public String toString() {
        return String.format("%s (%s, %d)", id, url, logPort);
    }

}
