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

import static com.google.appengine.api.labs.taskqueue.TaskOptions.Builder.url;

import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.labs.taskqueue.Queue;
import com.google.appengine.api.labs.taskqueue.QueueFactory;
import com.google.appengine.api.labs.taskqueue.TaskOptions;

import com.gargoylesoftware.htmlunit.BrowserVersion;
import com.gargoylesoftware.htmlunit.WebClient;
import com.gargoylesoftware.htmlunit.html.HtmlAnchor;
import com.gargoylesoftware.htmlunit.html.HtmlPage;
import com.unnison.ajaxcrawler.model.Host;
import com.unnison.ajaxcrawler.model.HostPlace;

import org.slim3.datastore.Datastore;

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

public class CrawlingTask extends HttpServlet {

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
        WebClient client = new WebClient(BrowserVersion.FIREFOX_3_6);
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
            // NOTE: Defold change
            // We don't have true google style hash-bang ajax url:s yet
            // We changed from #! to #
            if (href.startsWith("#")) {
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
                    queues.add(url("/crawlingtask").param("url", baseUrl + "/" + href));
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
