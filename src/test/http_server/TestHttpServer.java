import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.ServletException;

import java.util.regex.*;
import java.io.*;

import org.eclipse.jetty.server.Server;
import org.eclipse.jetty.server.Request;
import org.eclipse.jetty.server.handler.AbstractHandler;
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
        else
        {
            response.setStatus(HttpServletResponse.SC_BAD_REQUEST);
            baseRequest.setHandled(true);
        }
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
            server.setHandler(new TestHttpServer());

            server.start();
            Thread.sleep(1000 * 300);
            System.out.println("ERROR: HTTP server wasn't terminated by the tests after 300 seconds. Quiting...");
            System.exit(1);
            //server.join();
        }
        catch (Throwable e)
        {
            System.out.println(e);
            System.exit(1);
        }
    }
}
