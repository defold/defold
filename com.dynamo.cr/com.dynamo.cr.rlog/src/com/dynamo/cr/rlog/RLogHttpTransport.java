package com.dynamo.cr.rlog;

import java.io.IOException;
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

    @Override
    public void send(Record record) throws IOException {
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
        } catch (IOException e) {
            throw e;
        }
    }

}
