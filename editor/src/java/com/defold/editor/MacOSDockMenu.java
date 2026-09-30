// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.defold.editor;

import com.sun.jna.Callback;
import com.sun.jna.Function;
import com.sun.jna.Library;
import com.sun.jna.NativeLibrary;
import com.sun.jna.Pointer;
import javafx.application.Platform;

import java.nio.charset.StandardCharsets;
import java.util.Map;

public final class MacOSDockMenu {
    // Cocoa keeps the native menu and callback addresses for the application's
    // lifetime. Keep the Java callbacks reachable for just as long.
    private static final MacOSDockMenu INSTANCE = new MacOSDockMenu();

    public interface MenuCallback extends Callback {
        Pointer invoke(Pointer receiver, Pointer selector, Pointer application);
    }

    public interface ActionCallback extends Callback {
        void invoke(Pointer receiver, Pointer selector, Pointer sender);
    }

    private NativeLibrary objc;
    private Function objcMsgSend;
    private Pointer menu;
    private Pointer menuItem;
    private Runnable openNewWindow;
    private final MenuCallback menuCallback = (receiver, selector, application) -> menu;
    private final ActionCallback actionCallback = (receiver, selector, sender) -> openNewWindow.run();

    private MacOSDockMenu() {}

    public static MacOSDockMenu install(Runnable openNewWindow) {
        checkEventThread();
        INSTANCE.openNewWindow = openNewWindow;
        if (INSTANCE.menu == null) {
            INSTANCE.createMenu();
        }
        return INSTANCE;
    }

    public void setLabel(String label) {
        checkEventThread();
        sendVoid(menuItem, "setTitle:", string(label));
    }

    private static void checkEventThread() {
        if (!Platform.isFxApplicationThread()) {
            throw new IllegalStateException("The macOS Dock menu must be accessed on the JavaFX thread");
        }
    }

    private void createMenu() {
        objc = NativeLibrary.getInstance("objc", Map.of(Library.OPTION_STRING_ENCODING, StandardCharsets.UTF_8.name()));
        objcMsgSend = objc.getFunction("objc_msgSend");
        Pointer application = send(objcClass("NSApplication"), "sharedApplication");
        Pointer delegate = send(application, "delegate");
        if (delegate == null) {
            throw new IllegalStateException("The macOS application delegate is not available");
        }
        Pointer delegateClass = objc.getFunction("object_getClass").invokePointer(new Object[]{delegate});

        // AWT's Taskbar.setMenu() does not install a Dock menu when JavaFX owns
        // NSApplication. Add only the missing Dock callback to its existing
        // delegate, preserving JavaFX's window, activation, and quit handling.
        addMethod(delegateClass, "defoldOpenNewWindow:", actionCallback, "v@:@");
        addMethod(delegateClass, "applicationDockMenu:", menuCallback, "@@:@");

        Pointer newMenu = send(send(objcClass("NSMenu"), "alloc"), "initWithTitle:", string(""));
        Pointer newItem = send(send(objcClass("NSMenuItem"), "alloc"), "initWithTitle:action:keyEquivalent:",
                string(""), selector("defoldOpenNewWindow:"), string(""));
        sendVoid(newItem, "setTarget:", delegate);
        sendVoid(newMenu, "addItem:", newItem);
        menu = newMenu;
        menuItem = newItem;
        sendVoid(newItem, "release"); // The menu retains its item.
        // Refresh Cocoa's delegate capabilities after adding the Dock callback.
        sendVoid(application, "setDelegate:", delegate);
    }

    private void addMethod(Pointer objcClass, String name, Callback callback, String types) {
        Pointer selector = selector(name);
        if (objc.getFunction("class_getInstanceMethod").invokePointer(new Object[]{objcClass, selector}) != null) {
            throw new IllegalStateException("The macOS application already implements: " + name);
        }
        byte added = (Byte) objc.getFunction("class_addMethod").invoke(Byte.TYPE,
                new Object[]{objcClass, selector, callback, types});
        if (added == 0) {
            throw new IllegalStateException("Cannot add macOS Dock menu method: " + name);
        }
    }

    private Pointer objcClass(String name) {
        return objc.getFunction("objc_getClass").invokePointer(new Object[]{name});
    }

    private Pointer selector(String name) {
        return objc.getFunction("sel_registerName").invokePointer(new Object[]{name});
    }

    private Pointer string(String value) {
        return send(objcClass("NSString"), "stringWithUTF8String:", value);
    }

    private Object[] messageArguments(Pointer receiver, String selector, Object... arguments) {
        Object[] result = new Object[arguments.length + 2];
        result[0] = receiver;
        result[1] = selector(selector);
        System.arraycopy(arguments, 0, result, 2, arguments.length);
        return result;
    }

    private Pointer send(Pointer receiver, String selector, Object... arguments) {
        return objcMsgSend.invokePointer(messageArguments(receiver, selector, arguments));
    }

    private void sendVoid(Pointer receiver, String selector, Object... arguments) {
        objcMsgSend.invokeVoid(messageArguments(receiver, selector, arguments));
    }
}
