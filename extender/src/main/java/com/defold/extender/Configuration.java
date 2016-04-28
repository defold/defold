package com.defold.extender;

import java.util.List;
import java.util.Map;

public class Configuration {
    private Map<String, Platform> platforms;
    private List<String> headers;

    public void setPlatforms(Map<String, Platform> platforms) {
        this.platforms = platforms;
    }

    public Map<String, Platform> getPlatforms() {
        return platforms;
    }

    public void setHeaders(List<String> headers) {
        this.headers = headers;
    }

    public List<String> getHeaders() {
        return headers;
    }
}
