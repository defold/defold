// Copyright 2020-2025 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.ServletException;

import java.net.*;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.regex.*;
import java.util.Enumeration;
import java.util.List;
import java.util.ArrayList;
import java.util.stream.Collectors;
import java.io.*;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;

import org.eclipse.jetty.http.HttpHeader;
import org.eclipse.jetty.http.HttpStatus;
import org.eclipse.jetty.server.Server;
import org.eclipse.jetty.server.Request;
import org.eclipse.jetty.server.Handler;
import org.eclipse.jetty.util.resource.Resource;
import org.eclipse.jetty.server.handler.AbstractHandler;
import org.eclipse.jetty.server.handler.HandlerList;
import org.eclipse.jetty.server.handler.ResourceHandler;

import org.eclipse.jetty.server.Connector;
import org.eclipse.jetty.server.ServerConnector;
import org.eclipse.jetty.util.ssl.SslContextFactory;
import org.eclipse.jetty.server.HttpConfiguration;
import org.eclipse.jetty.server.HttpConnection;
import org.eclipse.jetty.server.HttpConnectionFactory;
import org.eclipse.jetty.server.SecureRequestCustomizer;


class TestSslSocketConnector extends ServerConnector
{
    public TestSslSocketConnector(Server server, SslContextFactory.Server sslContextFactory, HttpConnectionFactory connectionFactory) {
        super(server, sslContextFactory, connectionFactory);
    }

    @Override
    public void accept(int acceptorID) throws IOException
    {
        try {
            Thread.sleep(10000); // milliseconds
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        super.accept(acceptorID);
    }
}

class Range
{
    public long start;
    public long end;
    public long length;
    public Range(long start, long end, long length) {
        this.start = start;
        this.end = end;
        this.length = length;
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

    public static void digestStreamRange(MessageDigest md, InputStream input, long start, long size) throws IOException
    {
        input.skip(start);
        byte[] buffer = new byte[1024];
        int bytesRead;
        long toCopy = size;
        while ((bytesRead = input.read(buffer)) != -1)
        {
            if (toCopy < bytesRead)
                bytesRead = (int)toCopy;

            md.update(buffer, 0, bytesRead);
            toCopy -= bytesRead;
        }
    }

    public static void copyStream(InputStream input, OutputStream output, long start, long size) throws IOException
    {
        input.skip(start);
        byte[] buffer = new byte[1024];
        int bytesRead;
        long toCopy = size;
        while ((bytesRead = input.read(buffer)) != -1)
        {
            if (toCopy < bytesRead)
                bytesRead = (int)toCopy;

            output.write(buffer, 0, bytesRead);
            toCopy -= bytesRead;
        }
    }

    private void sendFile(String target, HttpServletResponse response) throws IOException {
        Reader r = new FileReader(target);
        char[] buf = new char[1024 * 128];
        int n = r.read(buf);
        if (n > 0) {
            response.getWriter().print(new String(buf, 0, n));
        }
        // NOTE: We flush here to force chunked encoding
        response.getWriter().flush();
        r.close();
    }

    private void sendFile(File file, Range range, HttpServletResponse response) throws IOException {
        long size = range.end - range.start + 1;
        InputStream is = new BufferedInputStream(new FileInputStream(file));
        OutputStream os = response.getOutputStream();
        copyStream(is, os, range.start, size);
        os.flush();
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

    private static String calculateSHA1(File file, List<Range> ranges) throws IOException {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-1");

            BufferedInputStream is = new BufferedInputStream(new FileInputStream(file));

            Range first_range = ranges.get(0);
            long size = first_range.end - first_range.start + 1;

            for (Range range : ranges) {
                digestStreamRange(md, is, range.start, range.end - range.start + 1);
            }

            is.close();

            return toHex(md.digest());

        } catch (IOException e) {
            return null;
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }
    }

    private static String calculateSHA1(File file) throws IOException {
        List<Range> ranges = new ArrayList<>();
        ranges.add(new Range(0, file.length()-1,file.length()));
        return calculateSHA1(file, ranges);
    }

    private void debugHeaders(HttpServletRequest request) {
        System.out.printf("HEADERS:\n");
        Enumeration headerNames = request.getHeaderNames();
        while (headerNames.hasMoreElements()) {
            String key = (String) headerNames.nextElement();
            String value = request.getHeader(key);
            System.out.printf("HEADER:  '%s: %s'\n", key, value);
        }
    }

    static private String dumpResponse(HttpServletResponse resp){
        StringBuilder sb = new StringBuilder();

        sb.append("Response Status = [" + resp.getStatus() + "], ");
        String headers = resp.getHeaderNames().stream()
                        .map(headerName -> headerName + " : " + resp.getHeaders(headerName) )
                        .collect(Collectors.joining(", "));

        if (headers.isEmpty()) {
            sb.append("Response headers: NONE,");
        } else {
            sb.append("Response headers: "+headers+",");
        }

        return sb.toString();
    }

    // https://gist.github.com/jneira/cf33844230f1ae4c22bf4a82d28d12c0
    List<Range> parseRanges(HttpServletRequest request, long length, HttpServletResponse response) throws IOException {
        Range full = new Range(0, length - 1, length);
        List<Range> ranges = new ArrayList<>();

        // Validate and process Range and If-Range headers.
        String range = request.getHeader("Range");
        if (range == null)
        {
            ranges.add(full);
            return ranges;
        }

        // Range header should match format "bytes=n-n,n-n,n-n...". If not, then return 416.
        if (!range.matches("^bytes=\\d*-\\d*(,\\d*-\\d*)*$")) {
            response.setHeader("Content-Range", "bytes */" + length); // Required in 416.
            response.sendError(HttpServletResponse.SC_REQUESTED_RANGE_NOT_SATISFIABLE);
            return null;
        }

        if (ranges.isEmpty()) {
            for (String part : range.substring(6).split(",")) {
                // Assuming a file with length of 100, the following examples returns bytes at:
                // 50-80 (50 to 80), 40- (40 to length=100), -20 (length-20=80 to length=100).

                String startStr = part.substring(0, part.indexOf("-"));
                String endStr = part.substring(part.indexOf("-")+1);
                long start = Integer.parseInt(startStr);
                long end = endStr.length() > 0 ? Integer.parseInt(endStr) : length;

                if (start == -1) {
                    start = length - end;
                    end = length - 1;
                } else if (end == -1 || end > length - 1) {
                    end = length - 1;
                }

                // Check if Range is syntactically valid. If not, then return 416.
                if (start > end) {
                    response.setHeader("Content-Range", "bytes */" + length); // Required in 416.
                    response.sendError(HttpServletResponse.SC_REQUESTED_RANGE_NOT_SATISFIABLE);
                    return ranges;
                }

                ranges.add(new Range(start, end, length));
            }
            return ranges;
        }
        return null;
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
            response.getWriter().print("TestHttpServer running!");
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
            response.setHeader(HttpHeader.ETAG.asString(), tag);
            int status = HttpServletResponse.SC_OK;

            String ifNoneMatch = request.getHeader(HttpHeader.IF_NONE_MATCH.asString());
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
            //debugHeaders(request);

            File contentFile = new File(target.substring(1));
            List<Range> ranges = parseRanges(request, contentFile.length(), response);

            Range first_range = ranges.get(0);
            long size = first_range.end - first_range.start + 1;
            boolean partial = size != first_range.length;

            String tag = String.format("W/\"" + calculateSHA1(contentFile, ranges) + "\"");
            response.setHeader(HttpHeader.ETAG.asString(), tag);
            int status = partial ? HttpServletResponse.SC_PARTIAL_CONTENT : HttpServletResponse.SC_OK;

            String ifNoneMatch = request.getHeader(HttpHeader.IF_NONE_MATCH.asString());
            if (ifNoneMatch != null) {
                Range range = ranges.get(0);
                if (ifNoneMatch.equals(tag)) {
                    baseRequest.setHandled(true);
                    status = HttpStatus.NOT_MODIFIED_304;
                }
            }

            if (status != HttpStatus.NOT_MODIFIED_304)
            {
                if (partial) {
                    response.setHeader("Content-Range", String.format("bytes %d-%d/%d", first_range.start, first_range.end, first_range.length));
                    response.setHeader("Content-Length", String.format("%d", size));
                } else {
                    response.setHeader("Content-Length", String.valueOf(first_range.length));
                }
            }

            response.setStatus(status);
            baseRequest.setHandled(true);

            // String s = dumpResponse(response);
            // System.out.printf("RESPONSE: '%s'\n", s);

            if (status == HttpServletResponse.SC_OK || status == HttpServletResponse.SC_PARTIAL_CONTENT) {
                if (ranges != null && ranges.size() > 0) {
                    Range range = ranges.get(0); // Currently we only support one range request
                    sendFile(contentFile, range, response);
                }
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
            sendFile(target.substring(1), response);
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
            HttpConnection.getCurrentConnection().getHttpChannel().getConnection().close();
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
            
            ServerConnector connector = new ServerConnector(server);
            connector.setIdleTimeout(10000); // millis
            server.addConnector(connector);

            SecureRequestCustomizer src = new SecureRequestCustomizer();
            src.setSniHostCheck(false);
            HttpConfiguration httpConfig = new HttpConfiguration();
            httpConfig.addCustomizer(src);
            HttpConnectionFactory connectionFactory = new HttpConnectionFactory(httpConfig);

            SslContextFactory.Server sslContextFactory = new SslContextFactory.Server();
            sslContextFactory.setSniRequired(false);
            sslContextFactory.setSslSessionTimeout(5); // seconds
            sslContextFactory.setKeyStorePath("src/test/data/keystore");
            sslContextFactory.setKeyStorePassword("defold");

            ServerConnector sslConnector = new ServerConnector(server, sslContextFactory, connectionFactory);
            sslConnector.setIdleTimeout(10000); // millis
            server.addConnector(sslConnector);

            TestSslSocketConnector testsslConnector = new TestSslSocketConnector(server, sslContextFactory, connectionFactory);
            testsslConnector.setIdleTimeout(10000); // millis
            server.addConnector(testsslConnector);

            HandlerList handlerList = new HandlerList();
            handlerList.addHandler(new TestHttpServer());
            ResourceHandler resourceHandler = new ResourceHandler() {

                @Override
                public void handle(String target, Request baseRequest, HttpServletRequest request, HttpServletResponse response) throws IOException, ServletException {
                    if (baseRequest.isHandled())
                        return;

                    String ifNoneMatch = request.getHeader(HttpHeader.IF_NONE_MATCH.asString());
                    if (ifNoneMatch != null) {
                        Resource resource = getResource(request.getServletPath());
                        if (resource != null && resource.exists()) {
                            File file = resource.getFile();
                            if (file != null) {
                                String thisEtag = String.format("%d", file.lastModified());
                                if (ifNoneMatch.equals(thisEtag)) {
                                    baseRequest.setHandled(true);
                                    response.setHeader(HttpHeader.ETAG.asString(), thisEtag);
                                    response.setStatus(HttpStatus.NOT_MODIFIED_304);
                                    return;
                                }
                            }
                        }
                    }

                    super.handle(target, baseRequest, request, response);
                }

            //     @Override
            //     protected void doResponseHeaders(HttpServletResponse response,
            //             Resource resource,
            //             String mimeType) {
            //         super.doResponseHeaders(response, resource, mimeType);
            //         try {
            //             File file = resource.getFile();
            //             if (file != null) {
            //                 response.setHeader(HttpHeader.ETAG.asString(), String.format("%d", file.lastModified()));
            //             }
            //         }
            //         catch(IOException e) {
            //             throw new RuntimeException(e);
            //         }
            //     }
            };
            resourceHandler.setResourceBase(".");
            resourceHandler.setAcceptRanges(true);
            handlerList.addHandler(resourceHandler);
            server.setHandler(handlerList);

            server.start();

            final Connector[] connectors = server.getConnectors();
            int port = ((ServerConnector)connectors[0]).getLocalPort();
            int port_ssl = ((ServerConnector)connectors[1]).getLocalPort();
            int port_ssl_test = ((ServerConnector)connectors[2]).getLocalPort();

            // Early exit if any of the connectors is not opened or closed.
            if ((port == -1) || (port == -2)) {
                System.out.println("ERROR: Connector 0 is not opened or closed!");
                return;
            }
            if ((port_ssl == -1) || (port_ssl == -2)) {
                System.out.println("ERROR: Connector 1 is not opened or closed!");
                return;
            }
            if ((port_ssl_test == -1) || (port_ssl_test == -2)) {
                System.out.println("ERROR: Connector 2 is not opened or closed!");
                return;
            }

            try {
                String tempname = "test_http_server.cfg.tmp";
                String filename = "test_http_server.cfg";

                PrintWriter writer = new PrintWriter(tempname, "UTF-8");
                writer.println("# These are the sockets the test server currently listens to");
                writer.println("[server]");
                writer.println(String.format("socket=%d", port));
                writer.println(String.format("socket_ssl=%d", port_ssl));
                writer.println(String.format("socket_ssl_test=%d", port_ssl_test));
                writer.close();

                File tempfile = new File(tempname);
                File file = new File(filename);

                try {
                    Files.move(Paths.get(tempname), Paths.get(filename), StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
                    System.out.println("File '" + filename + "' created");
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
