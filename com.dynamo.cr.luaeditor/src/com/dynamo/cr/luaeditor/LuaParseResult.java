package com.dynamo.cr.luaeditor;

public class LuaParseResult {

    private String namepace = "";
    private String function = "";
    private boolean inFunction = false;
    private int matchStart = 0;
    private int matchEnd = 0;

    public LuaParseResult() {
    }

    public LuaParseResult(String namespace, String function, boolean inFunction, int matchStart, int matchEnd) {
        this.namepace = namespace;
        this.function = function;
        this.inFunction = inFunction;
        this.matchStart = matchStart;
        this.matchEnd  = matchEnd;
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

    public int getMatchStart() {
        return matchStart;
    }

    public int getMatchEnd() {
        return matchEnd;
    }

}
