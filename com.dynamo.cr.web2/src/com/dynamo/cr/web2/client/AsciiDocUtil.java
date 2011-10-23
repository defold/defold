package com.dynamo.cr.web2.client;

import com.google.gwt.regexp.shared.MatchResult;
import com.google.gwt.regexp.shared.RegExp;
import com.google.gwt.user.client.ui.HTML;

public class AsciiDocUtil {

    public static HTML extractBody(String html) {
        RegExp pattern = RegExp.compile("[\\s\\S]*?<body[\\s\\S]*?>([\\s\\S]*?)</body>[\\s\\S]*?");
        MatchResult result = pattern.exec(html);
        if (result != null) {
            return new HTML("<div class=\"asciidoc\">"  + result.getGroup(1) + "</div>");
        } else {
            return new HTML("Invalid file.");
        }
    }
}
