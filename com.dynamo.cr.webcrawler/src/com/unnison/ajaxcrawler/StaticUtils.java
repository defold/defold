/*
 * Copyright 2010 Gal Dolber.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */
package com.unnison.ajaxcrawler;

import com.unnison.ajaxcrawler.model.Host;
import com.unnison.ajaxcrawler.model.HostPlace;

import java.io.IOException;
import java.net.URL;
import java.net.URLDecoder;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

public class StaticUtils {

    static final String UTF_8 = "UTF-8";
    static final String _ESCAPED_FRAGMENT = "_escaped_fragment_";

    private static final long ONE_DAY = 1000 * 60 * 60 * 24;

    public static String parse(String urlString) throws IOException {
        URL url = new URL(urlString);

        String query = url.getQuery();

        String _escaped_fragment_ = "";
        StringBuilder sb = new StringBuilder();

        // Parse params
        if (query != null) {
            Map<String, List<String>> params = new HashMap<String, List<String>>();
            for (String param : query.split("&")) {
                int index = param.indexOf("=");
                String key = URLDecoder.decode(param.substring(0, index), UTF_8);
                String value = URLDecoder.decode(param.substring(index + 1, param.length()), UTF_8);
                List<String> values = params.get(key);
                if (values == null) {
                    values = new ArrayList<String>();
                    params.put(key, values);
                }
                values.add(value);
            }

            // Look up for the escaped fragment
            if (params.containsKey(_ESCAPED_FRAGMENT)) {
                List<String> list = params.get(_ESCAPED_FRAGMENT);
                if (list.size() > 1) {
                    throw new IllegalStateException();
                }
                _escaped_fragment_ = list.get(0);
                params.remove(_ESCAPED_FRAGMENT);
            }

            // Rebuild the params
            for (Entry<String, List<String>> entry : params.entrySet()) {
                for (String value : entry.getValue()) {
                    if (sb.length() > 0) {
                        sb.append("&");
                    }

                    sb.append(URLEncoder.encode(entry.getKey(), UTF_8));
                    sb.append("=");
                    sb.append(URLEncoder.encode(value, UTF_8));
                }
            }
        }

        String ref = url.getRef();
        ref = ref == null ? "" : ref;

        String rebuildUrl = url.getProtocol() + "://" + url.getHost() + url.getPath();

        // Cut out the last /
        if (rebuildUrl.endsWith("/")) {
            rebuildUrl = rebuildUrl.substring(0, rebuildUrl.length() - 1);
        }

        int index = rebuildUrl.indexOf("?");
        if (index != -1) {
            rebuildUrl = rebuildUrl.substring(0, index);
        }

        rebuildUrl += (sb.length() != 0 ? "?" + sb.toString() : "")
            + (!_escaped_fragment_.isEmpty() ? "#!" + _escaped_fragment_ : (ref.isEmpty() ? "" : "#" + ref));

        return rebuildUrl;
    }

    public static boolean isExpired(HostPlace hostPlace, Host host) {
        Date lastFetch = hostPlace != null ? hostPlace.getLastFetch() : host.getLastFetch();
        if (lastFetch == null) {
            return true;
        }

        Date today = new Date();
        long delta = today.getTime() - lastFetch.getTime();
        if (delta < host.getFetchRatio() * ONE_DAY) {
            return false;
        } else {
            return true;
        }
    }
}
