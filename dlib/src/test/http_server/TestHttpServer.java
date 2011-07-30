import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.ServletException;

import java.util.regex.*;
import java.io.*;

import org.eclipse.jetty.http.HttpHeaders;
import org.eclipse.jetty.http.HttpStatus;
import org.eclipse.jetty.server.Server;
import org.eclipse.jetty.server.Request;
import org.eclipse.jetty.server.Handler;
import org.eclipse.jetty.util.resource.Resource;
import org.eclipse.jetty.server.handler.AbstractHandler;
import org.eclipse.jetty.server.handler.HandlerList;
import org.eclipse.jetty.server.handler.ResourceHandler;
import org.eclipse.jetty.server.bio.SocketConnector;

public class TestHttpServer extends AbstractHandler
{
    Pattern m_AddPattern = Pattern.compile("/add/(\\d+)/(\\d+)");
    Pattern m_ArbPattern = Pattern.compile("/arb/(\\d+)");
    public TestHttpServer()
    {
        super();
    }

    public void handle(String target,
                       Request baseRequest,
                       HttpServletRequest request,
                       HttpServletResponse response)
        throws IOException, ServletException
    {
        Matcher addm = m_AddPattern.matcher(target);
        Matcher arbm = m_ArbPattern.matcher(target);

        if (target.equals("/"))
        {
            response.setStatus(HttpServletResponse.SC_OK);
            baseRequest.setHandled(true);
        }
        else if (target.equals("/cached"))
        {
            String tag = String.format("W/\"A TAG\"");
            response.setHeader(HttpHeaders.ETAG, tag);
            int status = HttpServletResponse.SC_OK;

            String ifNoneMatch = request.getHeader(HttpHeaders.IF_NONE_MATCH);
            if (ifNoneMatch != null) {
                if (ifNoneMatch.equals(tag)) {
                    baseRequest.setHandled(true);
                    status = HttpStatus.NOT_MODIFIED_304;
                }
            }

            response.setStatus(status);
            baseRequest.setHandled(true);
            if (status == HttpServletResponse.SC_OK)
                response.getWriter().print("cached_content");
        }
        else if (addm.matches())
        {
            response.setStatus(HttpServletResponse.SC_OK);
            baseRequest.setHandled(true);
            response.getWriter().println(Integer.parseInt(addm.group(1)) + Integer.parseInt(addm.group(2)));
        }
        else if (arbm.matches())
        {
            response.setStatus(HttpServletResponse.SC_OK);
            baseRequest.setHandled(true);

            int n = Integer.parseInt(arbm.group(1));
            // NOTE: If content lenght is not set here chunked transfer encoding might be enabled (Transfer-Encoding: chunked)
            response.setContentLength(n);

            byte[] buffer = new byte[n];

            StringBuffer sb = new StringBuffer();
            for (int i = 0; i < n; ++i)
                buffer[i] = (byte) ((i % 255) & 0xff);
            response.getOutputStream().write(buffer);
        }
        else if (target.equals("/src/test/data/test.config")) {
            // For dmConfigFile test
            response.setStatus(HttpServletResponse.SC_OK);
            baseRequest.setHandled(true);
            Reader r = new FileReader(target.substring(1));
            char[] buf = new char[1024 * 128];
            int n = r.read(buf);
            response.getWriter().println(new String(buf, 0, n));
            r.close();
        }
        else if (target.equals("/post")) {
            baseRequest.setHandled(true);
            if (!request.getMethod().equals("POST")) {
                response.setStatus(HttpServletResponse.SC_BAD_REQUEST );
                return;
            }

            InputStream is = request.getInputStream();
            byte[] buf = new byte[1024];
            int n = is.read(buf);
            int sum = 0;
            while (n != -1) {
                for (int i = 0; i < n; ++i) {
                    sum += buf[i];
                }
                n = is.read(buf);
            }
            is.close();

            response.getWriter().println(sum);
            response.setStatus(HttpServletResponse.SC_OK);
        }
        // No match? Let ResourceHandler handle the request. See setup code.
    }

    public static void main(String[] args) throws Exception
    {
        try
        {
            Server server = new Server();
            SocketConnector connector = new SocketConnector();
            connector.setMaxIdleTime(100);
            connector.setPort(7000);
            server.addConnector(connector);
            HandlerList handlerList = new HandlerList();
            handlerList.addHandler(new TestHttpServer());
            ResourceHandler resourceHandler = new ResourceHandler() {

                @Override
                public void handle(String target, Request baseRequest, HttpServletRequest request, HttpServletResponse response) throws IOException, ServletException {
                    if (baseRequest.isHandled())
                        return;

                    String ifNoneMatch = request.getHeader(HttpHeaders.IF_NONE_MATCH);
                    if (ifNoneMatch != null) {
                        Resource resource = getResource(request);
                        if (resource != null && resource.exists()) {
                            File file = resource.getFile();
                            if (file != null) {
                                String thisEtag = String.format("%d", file.lastModified());
                                if (ifNoneMatch.equals(thisEtag)) {
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
                            response.setHeader(HttpHeaders.ETAG, String.format("%d", file.lastModified()));
                        }
                    }
                    catch(IOException e) {
                        throw new RuntimeException(e);
                    }
                }
            };
            resourceHandler.setResourceBase(".");
            handlerList.addHandler(resourceHandler);
            server.setHandler(handlerList);

            server.start();
            Thread.sleep(1000 * 300);
            System.out.println("ERROR: HTTP server wasn't terminated by the tests after 300 seconds. Quiting...");
            System.exit(1);
        }
        catch (Throwable e)
        {
            System.out.println(e);
            System.exit(1);
        }
    }
}
