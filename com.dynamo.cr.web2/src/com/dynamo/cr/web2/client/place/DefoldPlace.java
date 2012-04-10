package com.dynamo.cr.web2.client.place;

import com.google.gwt.place.shared.Place;

public abstract class DefoldPlace extends Place {

    public static String niceify(String string) {
        String[] lst = string.split("_");
        String ret = "";
        int i = 0;
        for (String s : lst) {
            ret += s.substring(0, 1).toUpperCase() + s.substring(1);
            if (i < lst.length - 1)
                ret += " ";
            i++;
        }

        return ret;
    }

    public abstract String getTitle();

}
