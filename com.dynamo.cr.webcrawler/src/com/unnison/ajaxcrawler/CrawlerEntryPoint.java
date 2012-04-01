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

import com.google.appengine.api.labs.taskqueue.QueueFactory;

import com.unnison.ajaxcrawler.model.Host;

import org.slim3.datastore.Datastore;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Date;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

public class CrawlerEntryPoint extends HttpServlet {

    @Override
    protected void doGet(HttpServletRequest req, HttpServletResponse resp) throws ServletException, IOException {
        String url = req.getParameter("url");
        if (url != null) {
            Host host = Datastore.getOrNull(Host.class, Datastore.createKey(Host.class, url));
            if (host != null) {
                fireCrawlingTask(host);
                resp.getWriter().print("The crawling is now running");
                return;
            }
        }
        resp.getWriter().print("Error");
    }

    public static void fireCrawlingTask(Host host) {
        if (StaticUtils.isExpired(null, host)) {
            ArrayList<String> indexes = host.getIndexes();
            for (String index : indexes) {
                QueueFactory.getDefaultQueue().add(
                    url("/crawlingtask").param("url", index).param("fetchRatio", String.valueOf(host.getFetchRatio())));
            }
            host.setLastFetch(new Date());
            Datastore.put(host);
        }
    }
}
