package com.dynamo.cr.web2.server;

import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.util.Map;

import javax.servlet.Filter;
import javax.servlet.FilterChain;
import javax.servlet.FilterConfig;
import javax.servlet.ServletException;
import javax.servlet.ServletOutputStream;
import javax.servlet.ServletRequest;
import javax.servlet.ServletResponse;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

public final class CrawlFilter implements Filter {

    private final String snapshootService = "http://defoldcrawler.appspot.com";

    @Override
    public void init(FilterConfig filterConfig) throws ServletException {
    }

    @Override
    public void destroy() {
    }

    @Override
    public void doFilter(ServletRequest request, ServletResponse response, FilterChain chain) throws IOException, ServletException {
        HttpServletRequest r = (HttpServletRequest) request;

        @SuppressWarnings("rawtypes")
        Map parameters = r.getParameterMap();
        if (!parameters.containsKey("_escaped_fragment_")) {
            chain.doFilter(request, response);
        } else {
            URL url = new URL(r.getRequestURL().toString());
            String reqUrl = String.format("http://%s/#!%s", url.getHost(), r.getParameter("_escaped_fragment_"));


            // Fetch the snapshot
            URL fetchUrl = new URL(snapshootService + "/ajaxcrawler?url=" + URLEncoder.encode(reqUrl, "UTF-8"));
            response.setContentType("text/html");
            response.setCharacterEncoding("ISO-8859-1");
            ServletOutputStream out = response.getOutputStream();

            InputStream in = fetchUrl(fetchUrl);

            if (in == null) {
                ((HttpServletResponse) response).setStatus(404);
                out.print("404");
                return;
            }

            final byte[] bytes = new byte[4096];

            int bytesRead = in.read(bytes);
            while (bytesRead > -1) {
                out.write(bytes, 0, bytesRead);
                bytesRead = in.read(bytes);
            }
        }
    }

    private InputStream fetchUrl(URL url) {
        try {
            HttpURLConnection connection;
            connection = (HttpURLConnection) url.openConnection();
            connection.setDoOutput(true);
            connection.setRequestMethod("GET");
            connection.setConnectTimeout(10000);
            if (connection.getResponseCode() == HttpURLConnection.HTTP_OK) {
                return (InputStream) connection.getContent();
            } else {
                return null;
            }
        } catch (Exception e) {
            return null;
        }
    }
}
