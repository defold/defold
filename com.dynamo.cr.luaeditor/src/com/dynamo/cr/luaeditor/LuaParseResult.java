package com.dynamo.cr.luaeditor;

public class LuaParseResult {

    private String namepace = "";
    private String function = "";
    private boolean inFunction = false;

    public LuaParseResult() {
    }

    public LuaParseResult(String namespace, String function, boolean inFunction) {
        this.namepace = namespace;
        this.function = function;
        this.inFunction = inFunction;
    }

    public String getNamespace() {
        return namepace;
    }

    public String getFunction() {
        return function;
    }

    public boolean inFunction() {
        return inFunction;
    }

}
