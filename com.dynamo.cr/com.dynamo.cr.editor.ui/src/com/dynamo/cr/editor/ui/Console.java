package com.dynamo.cr.editor.ui;

import java.io.OutputStream;

import org.eclipse.ui.console.MessageConsole;

import com.dynamo.cr.editor.core.IConsole;

public class Console implements IConsole {
    private MessageConsole console;

    public Console(MessageConsole console) {
        this.console = console;
    }

    @Override
    public OutputStream createOutputStream() {
        return this.console.newOutputStream();
    }

}
