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

import org.slim3.datastore.Datastore;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

public class Register extends HttpServlet {

    @Override
    protected void doGet(HttpServletRequest req, HttpServletResponse resp) throws ServletException, IOException {
        String url = req.getParameter("url");
        String index = req.getParameter("index");
        if (url != null && index != null) {
            
            // Parse indexes
            ArrayList<String> indexes = new ArrayList<String>();
            if (index.indexOf(",") != -1) {
                indexes.addAll(Arrays.asList(index.split(",")));
            } else {
                indexes.add(index);
            }
            
            Host host = new Host();
            host.setKey(Datastore.createKey(Host.class, url));
            host.setFetchRatio(7); // 1 week by default (edit in the datastore viewer)
            host.setIndexes(indexes);
            
            Datastore.put(host);

            CrawlerEntryPoint.fireCrawlingTask(host);
            
            resp.getWriter().print("Registered ok!");
        } else {
            resp.getWriter().print("Url or Index not specified");
        }
    }
}
