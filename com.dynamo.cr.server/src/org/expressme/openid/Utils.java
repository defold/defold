package org.expressme.openid;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.GZIPInputStream;

/**
 * Utils for OpenID authentication.
 * 
 * @author Michael Liao (askxuefeng@gmail.com)
 */
class Utils {

    private static final int MAX_SIZE = 10240;
    private static final String CONTENT = "Content";

    static Map<String, Object> httpRequest(String url, String method, String accept, String formPostData, int timeOutInMilliseconds) {
        HttpURLConnection hc = null;
        InputStream input = null;
        OutputStream output = null;
        try {
            hc = (HttpURLConnection) new URL(url).openConnection();
            hc.setRequestMethod(method);
            hc.setDoOutput("POST".equals(method));
            hc.setUseCaches(false);
            hc.setConnectTimeout(timeOutInMilliseconds);
            hc.setReadTimeout(timeOutInMilliseconds);
            hc.addRequestProperty("Accept", accept);
            hc.addRequestProperty("Accept-Encoding", "gzip");
            if (hc.getDoOutput()) {
                output = hc.getOutputStream();
                output.write(formPostData.getBytes("UTF-8"));
                output.flush();
            }
            hc.connect();
            int code = hc.getResponseCode();
            if (code != 200)
                throw new OpenIdException("Bad response code: " + code);
            // handle content:
            // try to detect encoding:
            boolean gzip = "gzip".equals(hc.getContentEncoding());
            input = hc.getInputStream();
            ByteArrayOutputStream byteArrayOutput = new ByteArrayOutputStream();
            boolean overflow = false;
            int read = 0;
            byte[] buffer = new byte[1024];
            for(;;) {
                int n = input.read(buffer);
                if(n==(-1))
                    break;
                byteArrayOutput.write(buffer, 0, n);
                read += n;
                if(read>MAX_SIZE) {
                    overflow = true;
                    break;
                }
            }
            byteArrayOutput.close();
            if(overflow)
                throw new RuntimeException();
            byte[] data = byteArrayOutput.toByteArray();
            if(gzip)
                data = gunzip(data);
            Map<String, Object> map = new HashMap<String, Object>();
            map.put("Cache-Control", hc.getHeaderField("Cache-Control"));
            map.put("Content-Type", hc.getHeaderField("Content-Type"));
            map.put("Content-Encoding", hc.getHeaderField("Content-Encoding"));
            map.put("Expires", hc.getHeaderField("Expires"));
            map.put(CONTENT, data);
            return map;
        }
        catch(IOException e) {
            throw new OpenIdException("Request failed: " + url, e);
        }
        finally {
            if(output!=null) {
                try { output.close(); } catch (IOException e) {}
            }
            if(input!=null) {
                try { input.close(); } catch (IOException e) {}
            }
            if(hc!=null) {
                hc.disconnect();
            }
        }
    }

    /**
     * Get 'max-age' field from HTTP header.
     */
    static long getMaxAge(Map<String, Object> map) {
        String cache = (String) map.get("Cache-Control");
        if (cache==null)
            return 0L;
        int pos = cache.indexOf("max-age=");
        if (pos!=(-1)) {
            String maxAge = cache.substring(pos + "max-age=".length());
            pos = maxAge.indexOf(',');
            if (pos!=(-1))
                maxAge = maxAge.substring(0, pos);
            try {
                return Integer.parseInt(maxAge) * 1000L;
            }
            catch (Exception e) {}
        }
        return 0L;
    }

    /**
     * Get text content from HTTP response.
     */
    static String getContent(Map<String, Object> map) throws UnsupportedEncodingException {
        byte[] data = (byte[]) map.get(CONTENT);
        String charset = getCharset(map);
        if (charset==null)
            charset = "UTF-8";
        return new String(data, charset);
    }

    /**
     * Unzip data using gzip.
     */
    public static byte[] gunzip(byte[] data) {
        ByteArrayOutputStream byteOutput = new ByteArrayOutputStream(10240);
        GZIPInputStream input = null;
        try {
            input = new GZIPInputStream(new ByteArrayInputStream(data));
            byte[] buffer = new byte[1024];
            int n = 0;
            for(;;) {
                n = input.read(buffer);
                if(n<=0)
                    break;
                byteOutput.write(buffer, 0, n);
            }
        }
        catch (IOException e) {
            throw new OpenIdException(e);
        }
        finally {
            if(input!=null) {
                try {
                    input.close();
                }
                catch(IOException ioe) {}
            }
        }
        return byteOutput.toByteArray();
    }

    /**
     * Get substring between start token and end token.
     */
    public static String mid(String s, String startToken, String endToken) {
        return mid(s, startToken, endToken, 0);
    }

    /**
     * Get substring between start token and end token, searching from specific index.
     */
    public static String mid(String s, String startToken, String endToken, int fromStart) {
        if (startToken==null || endToken==null)
            return null;
        int start = s.indexOf(startToken, fromStart);
        if (start==(-1))
            return null;
        int end = s.indexOf(endToken, start + startToken.length());
        if (end==(-1))
            return null;
        String sub = s.substring(start + startToken.length(), end);
        return sub.trim();
    }

    /**
     * Get 'charset' field from HTTP header.
     */
    public static String getCharset(Map<String, Object> map) {
        String contentType = (String) map.get("Content-Type");
        String charset = null;
        if(contentType!=null) {
            int pos = contentType.indexOf("charset=");
            if(pos!=(-1)) {
                charset = contentType.substring(pos + "charset=".length());
                int sp = contentType.indexOf(';');
                if(sp!=(-1))
                    contentType = contentType.substring(0, sp).trim();
            }
        }
        return charset;
    }

    /**
     * Encode URL with UTF-8.
     */
    public static String urlEncode(String s) throws UnsupportedEncodingException {
        return URLEncoder.encode(s, "UTF-8");
    }

    /**
     * Build query string like "a=1&b=2&c=3".
     */
    public static String buildQuery(List<String> list) {
        if (!list.isEmpty()) {
            StringBuilder sb = new StringBuilder(1024);
            try {
                for (String s : list) {
                    int n = s.indexOf('=');
                    sb.append(s.substring(0, n+1))
                      .append(URLEncoder.encode(s.substring(n+1), "UTF-8"))
                      .append('&');
                }
                // remove last '&':
                sb.deleteCharAt(sb.length()-1);
            }
            catch(UnsupportedEncodingException e) {
                throw new OpenIdException(e);
            }
            return sb.toString();
        }
        return "";
    }

    private static final char[] HEX = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    /**
     * Encode bytes as hex string.
     */
    public static String toHexString(byte[] bytes) {
        int length = bytes.length;
        StringBuffer sb = new StringBuffer(length * 2);
        int x = 0;
        int n1 = 0, n2 = 0;
        for(int i=0; i<length; i++) {
            if(bytes[i]>=0)
                x = bytes[i];
            else
                x= 256 + bytes[i];
            n1 = x >> 4;
            n2 = x & 0x0f;
            sb = sb.append(HEX[n1]);
            sb = sb.append(HEX[n2]);
        }
        return sb.toString();
    }

}
