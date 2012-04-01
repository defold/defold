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

import com.google.appengine.api.datastore.Entity;
import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.datastore.Text;

import com.unnison.ajaxcrawler.meta.HostPlaceMeta;
import com.unnison.ajaxcrawler.model.Host;
import com.unnison.ajaxcrawler.model.HostPlace;

import org.slim3.datastore.Datastore;

import java.io.IOException;
import java.net.URL;

import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

@SuppressWarnings("serial")
public class GetSnapshot extends HttpServlet {

    @Override
    public void doGet(HttpServletRequest req, HttpServletResponse resp) throws IOException {
        String url = req.getParameter("url");
        if (url == null) {
            resp.getWriter().append("Url not specified");
            return;
        }
        
        url = StaticUtils.parse(url);

        resp.setContentType("text/html");

        Key hostKey = Datastore.createKey(Host.class, new URL(url).getHost());
        Host host = Datastore.get(Host.class, hostKey);
        
        Entity hostPlace = Datastore.getOrNull(Datastore.createKey(hostKey, HostPlace.class, url));
        if (hostPlace != null) {
            resp.getWriter().append(((Text) hostPlace.getProperty(HostPlaceMeta.get().html.getName())).getValue());
        } else {
            // If not found we return the first index
            Entity homePlace = Datastore.getOrNull(Datastore.createKey(hostKey, HostPlace.class, host.getIndexes().get(0)));
            
            if (homePlace != null) {
                resp.getWriter().append(((Text) homePlace.getProperty(HostPlaceMeta.get().html.getName())).getValue());
            } else {
                
                // If the home does not exists
                throw new IOException();
            }
        }
        
        CrawlerEntryPoint.fireCrawlingTask(host);
    }
}
