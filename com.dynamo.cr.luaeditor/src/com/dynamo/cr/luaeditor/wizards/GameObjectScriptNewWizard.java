package com.dynamo.cr.luaeditor.wizards;


public class GameObjectScriptNewWizard extends AbstractScriptNewWizard {

    @Override
    public String getTitle() {
        return "Script file";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new script file.";
    }

    @Override
    public String getExtension() {
        return "script";
    }

    @Override
    public String getTemplateResourceName() {
        return "com/dynamo/cr/luaeditor/wizards/template.script";
    }
}
