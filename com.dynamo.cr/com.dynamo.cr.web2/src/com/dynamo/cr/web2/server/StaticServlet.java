package com.dynamo.cr.web2.server;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.logging.Level;

import javax.servlet.ServletContext;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import com.google.appengine.api.memcache.ErrorHandlers;
import com.google.appengine.api.memcache.MemcacheService;
import com.google.appengine.api.memcache.MemcacheServiceFactory;

/**
 * Servlet for serving static content without the trailing .html
 * Memcache is used to cache pages. (necessary?)
 * @author chmu
 *
 */
public class StaticServlet extends HttpServlet {
    private static final long serialVersionUID = -7462855764457995784L;
    private MemcacheService syncCache;

    public StaticServlet() {
        super();
        syncCache = MemcacheServiceFactory.getMemcacheService();
        syncCache.setErrorHandler(ErrorHandlers.getConsistentLogAndContinue(Level.INFO));
    }

    public void doGet(HttpServletRequest req, HttpServletResponse resp)
            throws IOException {

        URL url = new URL(req.getRequestURL().toString());

        ServletContext context = getServletContext();
        String path = String.format("%s.html", url.getPath());

        byte[] page = (byte[]) syncCache.get(path);
        if (page == null) {
            InputStream stream = context.getResourceAsStream(path);
            if (stream == null) {
                resp.sendError(HttpServletResponse.SC_NOT_FOUND);
                return;
            }

            page = loadResource(stream);
            syncCache.put(path, page);
        }
        resp.setContentType("text/html");
        resp.getOutputStream().write(page);
    }

    private byte[] loadResource(InputStream stream) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream(1024 * 128);
        byte[] buf = new byte[1024 * 128];
        int n = stream.read(buf);
        while (n > 0) {
            output.write(buf, 0, n);
            n = stream.read(buf);
        }
        stream.close();
        output.close();
        return output.toByteArray();
    }
}
