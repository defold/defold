package com.dynamo.cr.web2.server;

import java.io.IOException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import com.dynamo.cr.web2.shared.Configuration;

@SuppressWarnings("serial")
public class ProjectRedirectServlet extends HttpServlet {

    Pattern pattern = Pattern.compile("/p/(.*?)");

    public void doGet(HttpServletRequest req, HttpServletResponse resp) throws IOException {
        String path = req.getRequestURI();
        Matcher m = pattern.matcher(path);
        if (m.matches()) {
            String url = String.format("%s/projects/%s", Configuration.url, m.group(1));
            resp.sendRedirect(url);
        } else {
            resp.setStatus(404);
            resp.getOutputStream().write("Not found".getBytes());
        }
    }
}

