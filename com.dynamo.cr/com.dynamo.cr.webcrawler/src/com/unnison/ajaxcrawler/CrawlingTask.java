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

import static com.google.appengine.api.taskqueue.TaskOptions.Builder.withUrl;

import java.io.IOException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashSet;
import java.util.List;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.slim3.datastore.Datastore;

import com.gargoylesoftware.htmlunit.BrowserVersion;
import com.gargoylesoftware.htmlunit.BrowserVersionFeatures;
import com.gargoylesoftware.htmlunit.WebClient;
import com.gargoylesoftware.htmlunit.html.HtmlAnchor;
import com.gargoylesoftware.htmlunit.html.HtmlPage;
import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.taskqueue.Queue;
import com.google.appengine.api.taskqueue.QueueFactory;
import com.google.appengine.api.taskqueue.TaskOptions;
import com.unnison.ajaxcrawler.model.Host;
import com.unnison.ajaxcrawler.model.HostPlace;

public class CrawlingTask extends HttpServlet {

    /**
     *
     */
    private static final long serialVersionUID = 3467414411679278488L;

    BrowserVersionFeatures[] browserFeatures = new BrowserVersionFeatures[] {
            // Copy-paste from FF3.6.properties in htmlunit
            BrowserVersionFeatures.CAN_INHERIT_CSS_PROPERTY_VALUES,
            BrowserVersionFeatures.CSS_DISPLAY_DEFAULT,
            BrowserVersionFeatures.DIALOGWINDOW_REFERER,
            BrowserVersionFeatures.DISPLAYED_COLLAPSE,
            BrowserVersionFeatures.EVENT_DOM_CONTENT_LOADED,
            BrowserVersionFeatures.EVENT_INPUT,
            BrowserVersionFeatures.EVENT_ONERROR_EXTERNAL_JAVASCRIPT,
            BrowserVersionFeatures.EVENT_ONLOAD_EXTERNAL_JAVASCRIPT,
            BrowserVersionFeatures.FORMFIELD_REACHABLE_BY_NEW_NAMES,
            BrowserVersionFeatures.GENERATED_151,
            BrowserVersionFeatures.GENERATED_152,
            BrowserVersionFeatures.GENERATED_153,
            BrowserVersionFeatures.GENERATED_154,
            BrowserVersionFeatures.GENERATED_155,
            BrowserVersionFeatures.GENERATED_156,
            BrowserVersionFeatures.GENERATED_157,
            BrowserVersionFeatures.GENERATED_158,
            BrowserVersionFeatures.GENERATED_160,
            BrowserVersionFeatures.GENERATED_161,
            BrowserVersionFeatures.GENERATED_162,
            BrowserVersionFeatures.GENERATED_163,
            BrowserVersionFeatures.GENERATED_164,
            BrowserVersionFeatures.GENERATED_165,
            BrowserVersionFeatures.GENERATED_166,
            BrowserVersionFeatures.GENERATED_167,
            BrowserVersionFeatures.GENERATED_168,
            BrowserVersionFeatures.GENERATED_169,
            BrowserVersionFeatures.GENERATED_170,
            BrowserVersionFeatures.GENERATED_172,
            BrowserVersionFeatures.GENERATED_173,
            BrowserVersionFeatures.GENERATED_174,
            BrowserVersionFeatures.GENERATED_175,
            BrowserVersionFeatures.GENERATED_176,
            BrowserVersionFeatures.GENERATED_177,
            BrowserVersionFeatures.HTMLCOLLECTION_IDENTICAL_IDS,
            BrowserVersionFeatures.HTMLELEMENT_ALIGN_INVALID,
            BrowserVersionFeatures.HTMLELEMENT_CLASS_ATTRIBUTE,
            BrowserVersionFeatures.HTMLIMAGE_NAME_VALUE_PARAMS,
            BrowserVersionFeatures.HTMLINPUT_DEFAULT_IS_CHECKED,
            BrowserVersionFeatures.HTMLSCRIPT_APPLICATION_JAVASCRIPT,
            BrowserVersionFeatures.HTMLSCRIPT_SRC_JAVASCRIPT,
            BrowserVersionFeatures.HTML_BODY_COLOR,
            BrowserVersionFeatures.HTTP_HEADER_HOST_FIRST,
            BrowserVersionFeatures.JAVASCRIPT_GET_ELEMENT_BY_ID_CASE_SENSITIVE,
            BrowserVersionFeatures.JAVASCRIPT_OBJECT_PREFIX,
            BrowserVersionFeatures.JS_FRAME_RESOLVE_URL_WITH_PARENT_WINDOW,
            BrowserVersionFeatures.NOSCRIPT_BODY_AS_TEXT,
            BrowserVersionFeatures.PROTOCOL_DATA,
            BrowserVersionFeatures.STORAGE_OBSOLETE,
            BrowserVersionFeatures.STRING_TRIM,
            BrowserVersionFeatures.STYLESHEET_HREF_EXPANDURL,
            BrowserVersionFeatures.STYLESHEET_HREF_STYLE_NULL,
            BrowserVersionFeatures.URL_MISSING_SLASHES,
    };


    @Override
    protected void doPost(HttpServletRequest req, HttpServletResponse resp) throws ServletException, IOException {

        String url = StaticUtils.parse(req.getParameter("url"));

        Key hostKey = Datastore.createKey(Host.class, new URL(url).getHost());

        Host host = Datastore.getOrNull(Host.class, hostKey);

        Key hostPlaceKey = Datastore.createKey(hostKey, HostPlace.class, url);

        HostPlace hostPlace = Datastore.getOrNull(HostPlace.class, hostPlaceKey);
        if (hostPlace == null) {
            hostPlace = new HostPlace();
            hostPlace.setKey(hostPlaceKey);
            hostPlace.setLastFetch(new Date());
        }

        // Quickly update the lastFetch date
        Datastore.put(hostPlace);

        // Fetch the contents
        // Create custom user-agent for crawling (DefoldCrawler/1.0)
        BrowserVersion browserVersion = new BrowserVersion(
                "Netscape", "5.0 (Windows; en-US)",
                "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.9.2.8) Gecko/20100722 DefoldCrawler/1.0",
                (float) 1.0,  browserFeatures);

        WebClient client = new WebClient(browserVersion);
        client.setCssEnabled(false);
        client.setThrowExceptionOnScriptError(false);
        client.setThrowExceptionOnFailingStatusCode(false);
        HtmlPage page = client.getPage(url);
        client.getJavaScriptEngine().pumpEventLoop(10000);
        hostPlace.setHtml(page.asXml());
        Datastore.put(hostPlace);

        // Remove the href from the url
        String baseUrl = url;
        int index = baseUrl.indexOf("#");
        if (index != -1) {
            baseUrl = baseUrl.substring(0, index);
        }

        // Scan for any ajax link in the page and crawl it
        List<HtmlAnchor> anchors = page.getAnchors();
        ArrayList<HostPlace> places = new ArrayList<HostPlace>();
        ArrayList<TaskOptions> queues = new ArrayList<TaskOptions>();

        HashSet<String> hrefs = new HashSet<String>();
        for (HtmlAnchor a : anchors) {
            String href = a.getHrefAttribute();
            if (href.startsWith("#!")) {
                hrefs.add(href);
            }
        }


        for (String href : hrefs) {
            Key placeKey = Datastore.createKey(hostKey, HostPlace.class, baseUrl + href);
            HostPlace place = Datastore.getOrNull(HostPlace.class, placeKey);
            if (place == null || StaticUtils.isExpired(place, host)) {
                if (place == null) {
                    place = new HostPlace();
                    place.setKey(placeKey);
                }

                // We put the place with the current date as lastFetch so another task wont try to fetch it
                place.setLastFetch(new Date());
                places.add(place);

                if (href.indexOf('/') == -1) {
                    // NOTE: Defold change
                    // Fragments with slashes resulted in invalid url
                    // We skip these, eg #/, #reference:engine/exit etc
                    // We should perhaps not use slashes in fragments?
                    queues.add(withUrl("/crawlingtask").param("url", baseUrl + "/" + href));
                }

                if (queues.size() == 10) {
                    System.out.println("Adding 10 tasks");
                    Queue queue = QueueFactory.getDefaultQueue();
                    queue.add(queues);
                    queues.clear();
                }
            }
        }

        Datastore.put(places);

        Queue queue = QueueFactory.getDefaultQueue();
        queue.add(queues);
    }
}
