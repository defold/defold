package com.dynamo.cr.target.sign;

import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class IdentityParser {

    private static Pattern pattern = Pattern.compile("\\s+\\d+\\)\\s+([0-9A-Z]+)\\s+\"(.*?)\"");

    public static String[] parse(String string) {

        Map<String, String> result = new HashMap<String, String>();

        for (String line : string.split("\n")) {
            Matcher matcher = pattern.matcher(line);
            if (matcher.matches()) {
                result.put(matcher.group(1), matcher.group(2));
            }
        }

        return result.values().toArray(new String[result.values().size()]);
    }

}
