package com.dynamo.cr.luaeditor.wizards;


public class GuiScriptNewWizard extends AbstractScriptNewWizard {

    @Override
    public String getTitle() {
        return "Gui Script File";
    }

    @Override
    public String getDescription() {
        return "This wizard creates a new gui script file.";
    }

    @Override
    public String getExtension() {
        return "gui_script";
    }

    @Override
    public String getTemplateResourceName() {
        return "com/dynamo/cr/luaeditor/wizards/template.gui_script";
    }
}
