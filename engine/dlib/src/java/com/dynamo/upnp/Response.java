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

package com.dynamo.upnp;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.StringTokenizer;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class Response {

    public final int major;
    public final int minor;
    public final int statusCode;
    public final String statusString;
    public final Map<String, String> headers;

    private final static Pattern pattern = Pattern.compile("HTTP/([0-9])\\.([0-9]) (.*?) (.*?)");

    public Response(int major, int minor, int statusCode, String statusString, Map<String, String> headers) {
        this.major = major;
        this.minor = minor;
        this.statusCode = statusCode;
        this.statusString = statusString;
        this.headers = Collections.unmodifiableMap(headers);
    }

    public static Response parse(String header) {
        StringTokenizer st = new StringTokenizer(header, "\r\n");
        Map<String, String> headers = new HashMap<String, String>();
        int i = 0;
        int major = -1;
        int minor = -1;
        int statusCode = -1;
        String statusString = null;

        while (st.hasMoreTokens()) {
            String token = st.nextToken();

            if (i == 0) {
                Matcher m = pattern.matcher(token);
                if (!m.matches()) {
                    return null;
                }
                major = Integer.parseInt(m.group(1));
                minor = Integer.parseInt(m.group(2));
                statusCode = Integer.parseInt(m.group(3));
                statusString = m.group(4);

            } else {
                int index = token.indexOf(":");
                if (index != -1) {
                    String key = token.substring(0, index).toUpperCase();
                    String value = token.substring(index+1).trim();
                    headers.put(key, value);
                }
            }
            ++i;
        }

        return new Response(major, minor, statusCode, statusString, headers);
    }

    @Override
    public String toString() {
        return String.format("HTTP/%d.%d %d %s", major, minor, statusCode, statusString);
    }

}
