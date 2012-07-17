package com.dynamo.cr.editor.ui;

import org.eclipse.ui.console.ConsolePlugin;
import org.eclipse.ui.console.IConsoleManager;
import org.eclipse.ui.console.MessageConsole;

import com.dynamo.cr.editor.core.IConsole;
import com.dynamo.cr.editor.core.IConsoleFactory;

public class ConsoleFactory implements IConsoleFactory {

    @Override
    public IConsole getConsole(String name) {
        ConsolePlugin plugin = ConsolePlugin.getDefault();
        IConsoleManager conMan = plugin.getConsoleManager();
        org.eclipse.ui.console.IConsole[] existing = conMan.getConsoles();
        MessageConsole console = null;
        for (int i = 0; i < existing.length; i++) {
            if (name.equals(existing[i].getName())) {
                console = (MessageConsole) existing[i];
                break;
            }
        }
        // no console found, so create a new one
        if (console == null) {
            console = new MessageConsole(name, null);
            conMan.addConsoles(new org.eclipse.ui.console.IConsole[] { console });
        }
        return new Console(console);
    }

}
