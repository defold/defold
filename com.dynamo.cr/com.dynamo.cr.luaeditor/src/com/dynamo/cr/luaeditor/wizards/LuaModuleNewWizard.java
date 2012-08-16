package com.dynamo.cr.luaeditor.wizards;


public class LuaModuleNewWizard extends AbstractScriptNewWizard {

    @Override
    public String getTitle() {
        return "Lua Module file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new lua module file.";
    }

    @Override
    public String getExtension() {
        return "lua";
    }

    @Override
    public String getTemplateResourceName() {
        return "com/dynamo/cr/luaeditor/wizards/template.lua";
    }
}
