package com.defold.extender;

import java.util.List;

public class Platform {

    private List<String> compileArgs;
    private String compile;
    private String link;
    private String lib;

    public void setCompileArgs(List<String> compileArgs) {
        this.compileArgs = compileArgs;
    }

    public List<String> getCompileArgs() {
        return compileArgs;
    }

    public void setCompile(String compile) {
        this.compile = compile;
    }

    public String getCompile() {
        return compile;
    }

    public void setLink(String link) {
        this.link = link;
    }

    public String getLink() {
        return link;
    }

    public void setLib(String lib) {
        this.lib = lib;
    }

    public String getLib() {
        return lib;
    }
}
