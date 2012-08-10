package com.dynamo.cr.rlog;

import java.io.InputStream;
import java.io.OutputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;

import org.apache.commons.io.IOUtils;

import com.dynamo.cr.rlog.proto.RLog.Record;

public class RLogHttpTransport implements IRLogTransport {

    private URL url;

    public RLogHttpTransport() {
        try {
            url = new URL("http://defoldlog.appspot.com/post");
        } catch (MalformedURLException e) {}
    }

    /**
     * Send logging record.
     * @param entry entry to send
     * @return true on succes
     */
    @Override
    public boolean send(Record record) {
        try {
            URLConnection conn = url.openConnection();
            conn.setDoOutput(true);
            OutputStream os = conn.getOutputStream();
            InputStream is = null;
            try {
                byte[] msg = record.toByteArray();
                os.write(msg);
                os.flush();

                is = conn.getInputStream();
                byte[] buf = new byte[1024];
                int n;
                do {
                    n = is.read(buf);
                } while (n > 0);
            } finally {
                IOUtils.closeQuietly(os);
                IOUtils.closeQuietly(is);
            }
        } catch (Throwable e) {
            // NOTE: DO NOT LOG HERE! :-)
            e.printStackTrace();
            return false;
        }
        return true;
    }

}
