package com.dynamo.cr.editor;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URLDecoder;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.eclipse.jetty.http.HttpHeaders;
import org.eclipse.jetty.http.HttpStatus;
import org.eclipse.jetty.server.Request;
import org.eclipse.jetty.server.handler.ResourceHandler;
import org.eclipse.jetty.util.resource.Resource;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.common.cache.ETagCache;

public class FileHandler extends ResourceHandler {
    private static Logger logger = LoggerFactory.getLogger(FileHandler.class);
    private ETagCache etagCache = new ETagCache(10000);

    @Override
    public void handle(String target, Request baseRequest, HttpServletRequest request, HttpServletResponse response) throws IOException, ServletException {
        if (baseRequest.isHandled())
            return;

        if (target.equals("/__verify_etags__")) {
            baseRequest.setHandled(true);

            if (!request.getMethod().equals("POST")) {
                response.setStatus(HttpServletResponse.SC_BAD_REQUEST );
                return;
            }

            StringBuffer responseBuffer = new StringBuffer();
            BufferedReader reader = new BufferedReader(new InputStreamReader(request.getInputStream()));

            try {
                String line = reader.readLine();
                while (line != null) {
                    int i = line.indexOf(' ');
                    URI uri;
                    try {
                        uri = new URI(line.substring(0, i));
                        uri = uri.normalize(); // http://foo.com//a -> http://foo.com/a
                    } catch (URISyntaxException e) {
                        logger.warn(e.getMessage(), e);
                        continue;
                    }

                    String etag = line.substring(i + 1);
                    Resource resource = getResource(uri.getPath());
                    if (resource != null && resource.exists() && resource.getFile() != null) {
                        String thisEtag = etagCache.getETag(resource.getFile());
                        if (etag.equals(thisEtag)) {
                            responseBuffer.append(line.substring(0, i));
                            responseBuffer.append('\n');
                        }
                    }
                    else {
                        // NOTE: Do not log here. The cache might contain cache entries from other
                        // projects that are validated here
                    }

                    line = reader.readLine();
                }
            } finally {
                reader.close();
            }

            response.getWriter().print(responseBuffer);
            response.setStatus(HttpServletResponse.SC_OK);
            return;
        }

        String ifNoneMatch = request.getHeader(HttpHeaders.IF_NONE_MATCH);
        if (ifNoneMatch != null) {
            Resource resource = getResource(request);
            if (resource != null && resource.exists()) {
                File file = resource.getFile();
                if (file != null) {
                    String thisEtag = etagCache.getETag(file);
                    if (thisEtag != null && ifNoneMatch.equals(thisEtag)) {
                        baseRequest.setHandled(true);
                        response.setHeader(HttpHeaders.ETAG, thisEtag);
                        response.setStatus(HttpStatus.NOT_MODIFIED_304);
                        return;
                    }
                }
            }
        }

        super.handle(target, baseRequest, request, response);
    }

    @Override
    protected void doResponseHeaders(HttpServletResponse response,
            Resource resource,
            String mimeType) {
        super.doResponseHeaders(response, resource, mimeType);
        try {
            File file = resource.getFile();
            if (file != null) {
                String etag = etagCache.getETag(file);
                if (etag != null) {
                    response.setHeader(HttpHeaders.ETAG, etag);
                }
            }
        }
        catch(IOException e) {
            throw new RuntimeException(e);
        }
    }

}
