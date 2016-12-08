import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.ServletException;

import java.net.*;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.regex.*;
import java.io.*;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;

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
import org.eclipse.jetty.server.ssl.SslSocketConnector;


class TestSslSocketConnector extends SslSocketConnector
{
	@Override
	public void accept(int acceptorID) throws IOException, InterruptedException
	{
        try {
            Thread.sleep(10000); // milliseconds
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        super.accept(acceptorID);
	}
}

public class TestHttpServer extends AbstractHandler
{
    // Max number of retries when starting the server.
    static int maxRetries = 3;

    Pattern m_AddPattern = Pattern.compile("/add/(\\d+)/(\\d+)");
    Pattern m_ArbPattern = Pattern.compile("/arb/(\\d+)");
    Pattern m_CachedPattern = Pattern.compile("/cached/(\\d+)");
    Pattern m_EchoPattern = Pattern.compile("/echo/(.*)");
    Pattern m_ClosePattern = Pattern.compile("/close");
    Pattern m_SleepPattern = Pattern.compile("/sleep/(\\d+)");
    public TestHttpServer()
    {
        super();
    }

    private void sendFile(String target, HttpServletResponse response) throws IOException {
        Reader r = new FileReader(target.substring(1));
        char[] buf = new char[1024 * 128];
        int n = r.read(buf);
        if (n > 0) {
            response.getWriter().print(new String(buf, 0, n));
        }
        // NOTE: We flush here to force chunked encoding
        response.getWriter().flush();
        r.close();
    }

    private static char convertDigit(int value) {

        value &= 0x0f;
        if (value >= 10)
            return ((char) (value - 10 + 'a'));
        else
            return ((char) (value + '0'));
    }

    public static String toHex(byte bytes[]) {
        StringBuffer sb = new StringBuffer(bytes.length * 2);
        for (int i = 0; i < bytes.length; i++) {
            sb.append(convertDigit((int) (bytes[i] >> 4)));
            sb.append(convertDigit((int) (bytes[i] & 0x0f)));
        }
        return (sb.toString());
    }

    private static String calculateSHA1(File file) throws IOException {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-1");
            BufferedInputStream is = new BufferedInputStream(
                    new FileInputStream(file));
            byte[] buffer = new byte[1024];
            int n = is.read(buffer);
            while (n != -1) {
                md.update(buffer, 0, n);
                n = is.read(buffer);
            }
            is.close();
            return toHex(md.digest());

        } catch (IOException e) {
            return null;
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }
    }


    public void handle(String target,
                       Request baseRequest,
                       HttpServletRequest request,
                       HttpServletResponse response)
        throws IOException, ServletException
    {
        Matcher addm = m_AddPattern.matcher(target);
        Matcher arbm = m_ArbPattern.matcher(target);
        Matcher cachedm = m_CachedPattern.matcher(target);
        Matcher echom = m_EchoPattern.matcher(target);
        Matcher closem = m_ClosePattern.matcher(target);
        Matcher sleepm = m_SleepPattern.matcher(target);

        if (target.equals("/"))
        {
            response.setStatus(HttpServletResponse.SC_OK);
            baseRequest.setHandled(true);
        }
        else if (target.equals("/__verify_etags__")) {
            baseRequest.setHandled(true);
            if (!request.getMethod().equals("POST")) {
                response.setStatus(HttpServletResponse.SC_BAD_REQUEST );
                return;
            }

            StringBuffer responseBuffer = new StringBuffer();

            BufferedReader reader = new BufferedReader(new InputStreamReader(request.getInputStream()));
            String line = reader.readLine();
            while (line != null) {
                int i = line.indexOf(' ');
                URI uri;
                try {
                    uri = new URI(URLDecoder.decode(line.substring(0, i), "UTF-8"));
                    uri = uri.normalize(); // http://foo.com//a -> http://foo.com/a
                } catch (URISyntaxException e) {
                    throw new RuntimeException(e);
                }
                String etag = line.substring(i + 1);

                String actualETag = String.format("W/\"" + calculateSHA1(new File(uri.getPath().substring(1))) + "\"");

                if (etag.equals(actualETag)) {
                    responseBuffer.append(line.substring(0, i));
                    responseBuffer.append('\n');
                }

                line = reader.readLine();
            }
            reader.close();

            response.getWriter().print(responseBuffer);
            response.setStatus(HttpServletResponse.SC_OK);

        }
        else if (cachedm.matches())
        {
            String id = cachedm.group(1);
            String tag = String.format("W/\"A TAG " + id + "\"");
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
        else if (target.equals("/max-age-cached")) {
            response.setHeader("Cache-Control", "max-age=1");
            response.getWriter().print(System.currentTimeMillis());
            response.setStatus(HttpServletResponse.SC_OK);
            baseRequest.setHandled(true);
        }
        else if (target.startsWith("/tmp/http_files"))
        {
            String tag = String.format("W/\"" + calculateSHA1(new File(target.substring(1))) + "\"");
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
            if (status == HttpServletResponse.SC_OK) {
                sendFile(target, response);
            }
        }

        else if (addm.matches())
        {
            response.setStatus(HttpServletResponse.SC_OK);
            baseRequest.setHandled(true);
            String xscaleStr = request.getHeader("X-Scale");
            int xscale = 1;
            if (xscaleStr != null) {
            	xscale = Integer.parseInt(xscaleStr);
            }
            int sum = Integer.parseInt(addm.group(1)) + Integer.parseInt(addm.group(2));
            response.getWriter().println(sum * xscale);
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
            sendFile(target, response);
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
        } else if (echom.matches()) {
            String s = echom.group(1);
            s = URLDecoder.decode(s, "UTF-8");
            baseRequest.setHandled(true);
            response.getWriter().print(s);
            response.setStatus(HttpServletResponse.SC_OK);
        } else if (target.equals("/no-keep-alive")) {
            response.setHeader("Connection", "close");
            baseRequest.setHandled(true);
            response.getWriter().print("will close connection now.");
            response.setStatus(HttpServletResponse.SC_OK);
        } else if (closem.matches()) {
            java.net.Socket socket = (java.net.Socket) baseRequest.getConnection().getEndPoint().getTransport();
            socket.setSoTimeout(1);
            socket.close();
        } else if (sleepm.matches()) {
            int t = Integer.parseInt(sleepm.group(1));
            try {
                Thread.sleep(t);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
            response.setStatus(HttpServletResponse.SC_OK);
            baseRequest.setHandled(true);
        }
        // No match? Let ResourceHandler handle the request. See setup code.
    }

    public static void StartServer() throws Exception
    {
        try
        {
            Server server = new Server();
            SocketConnector connector = new SocketConnector();
            connector.setMaxIdleTime(500);
            server.addConnector(connector);

            SslSocketConnector sslConnector = new SslSocketConnector();
            sslConnector.setHandshakeTimeout(500);
            sslConnector.setMaxIdleTime(500);
            sslConnector.setKeystore("src/test/data/keystore");
            sslConnector.setKeyPassword("defold");
            server.addConnector(sslConnector);

            TestSslSocketConnector testsslConnector = new TestSslSocketConnector();
            testsslConnector.setHandshakeTimeout(500);
            testsslConnector.setMaxIdleTime(500);
            testsslConnector.setKeystore("src/test/data/keystore");
            testsslConnector.setKeyPassword("defold");
            server.addConnector(testsslConnector);

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

            for (int i = 0; i < server.getConnectors().length; ++i) {
                int port = server.getConnectors()[i].getLocalPort();
                // Early exit if any of the connectors is not opened or closed.
                if (port == -1)  {
                    System.out.println("ERROR: Connector " + i + " is not opened!");
                    return;
                } else if (port == -2)  {
                    System.out.println("ERROR: Connector " + i + " is closed!");
                    return;
                }
            }

            try {
                String tempname = "test_http_server.cfg.tmp";
                String filename = "test_http_server.cfg";

                PrintWriter writer = new PrintWriter(tempname, "UTF-8");
                writer.println("# These are the sockets the test server currently listens to");
                writer.println("[server]");
                writer.println(String.format("socket=%d", server.getConnectors()[0].getLocalPort()));
                writer.println(String.format("socket_ssl=%d", server.getConnectors()[1].getLocalPort()));
                writer.println(String.format("socket_ssl_test=%d", server.getConnectors()[2].getLocalPort()));
                writer.close();

                File tempfile = new File(tempname);
                File file = new File(filename);

                try {
                    Files.move(Paths.get(tempname), Paths.get(filename), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
                }
                catch(IOException e) {
                    System.out.println("ERROR: Failed to move " + tempname + " to " + filename);
                    throw new RuntimeException(e);
                }

            }
            catch(IOException e) {
                throw new RuntimeException(e);
            }

            Thread.sleep(1000 * 500);
            System.out.println("ERROR: HTTP server wasn't terminated by the tests after 500 seconds. Quitting...");
            System.exit(1);
        }
        catch (Throwable e)
        {
            System.out.println(e);
            System.exit(1);
        }
    }

    public static void main(String[] args) throws Exception
    {
        for (int i = 0; i < maxRetries; ++i) {
            StartServer(); // Will do a hard exit if it succeeds, if it returns we retry.
            System.out.println("Retrying to start server...");
        }
    }


}
