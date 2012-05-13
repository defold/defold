package com.dynamo.cr.web2.shared;

import com.google.gwt.user.client.Window;
import com.google.gwt.user.client.Window.Navigator;

public class ClientUtil {

    public enum Platform {
        Win32,
        OSX,
        Linux,
        Undefined
    };

    public enum Browser {
        Chrome,
        Safari,
        Firefox,
        Explorer {
            @Override
            public String toString() {
                return "Internet Explorer";
            }
        },
        Unknown,
    };

    public static native String getBrowserString() /*-{
        return $wnd.BrowserDetect.browser;
    }-*/;

    public static native double getBrowserVersion() /*-{
        return $wnd.BrowserDetect.version;
    }-*/;

    public static Browser getBrowser() {
        String b = getBrowserString();
        if (b.equals("Chrome")) {
            return Browser.Chrome;
        } else if (b.equals("Safari")) {
            return Browser.Safari;
        } else if (b.equals("Firefox")) {
            return Browser.Firefox;
        } else if (b.equals("Explorer")) {
            return Browser.Explorer;
        } else {
            return Browser.Unknown;
        }
    }

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

    public static boolean isDev() {
        return Window.Location.getHostName().equals("127.0.0.1");
    }
}
