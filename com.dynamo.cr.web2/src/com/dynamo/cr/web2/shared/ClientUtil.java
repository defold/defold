package com.dynamo.cr.web2.shared;

import com.google.gwt.user.client.Window.Navigator;

public class ClientUtil {

    public enum Platform {
        Win32,
        OSX,
        Linux,
        Undefined
    };

    public static Platform detectPlatform() {
        String platform = Navigator.getPlatform().toLowerCase();
        if (platform.contains("win")) {
            return Platform.Win32;
        } else if (platform.contains("mac")) {
            return Platform.OSX;
        } else if (platform.contains("linux")) {
            return Platform.Linux;
        } else {
            return Platform.Undefined;
        }
    }
}
