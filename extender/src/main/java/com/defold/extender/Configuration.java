package com.defold.extender;

import java.util.Map;

public class Configuration {
    public Map<String, PlatformConfig> platforms;
    public Map<String, String> context;
    public String[] exportedSymbols;
    public String[] includes;
}
