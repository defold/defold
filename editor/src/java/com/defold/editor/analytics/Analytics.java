package com.defold.editor.analytics;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;
import org.codehaus.jackson.node.ObjectNode;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLConnection;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.UUID;
import java.net.URLEncoder;


/*
 Notes and limitations
    - The log isn't persistent as it's complicated with multiple processes
 */

public class Analytics extends Thread {

    private String tid;
    private String applicationVersion;
    private String cid = UUID.randomUUID().toString();
    private List<String[][]> events = new LinkedList<>();
    private static int SEND_INTERVAL = 300;
    private static int MAX_BATCH = 16;
    private static int MAX_EVENTS = 50000;
    private static int MIN_SEND_INTERVAL = 1000;
    private static String GA_URL = "http://www.google-analytics.com/batch";
    private long lastSend = 0;
    private boolean running = true;
    private static Logger LOGGER = LoggerFactory.getLogger(Analytics.class);

    public Analytics(String applicationPath, String tid, String applicationVersion) {
        this.tid = tid;
        this.applicationVersion = applicationVersion;
        String path = String.format("%s/.defold-analytics", applicationPath);

        String tmpCid = loadCID(path);
        if (tmpCid != null) {
            cid = tmpCid;
        }
        writeCID(path, cid);
        start();
    }

    private static String loadCID(String path) {
        ObjectMapper om = new ObjectMapper();
        File file = new File(path);
        if (file.exists()) {
            try {
                JsonNode tree = om.readTree(file);
                String cid = tree.get("cid").getTextValue();
                return cid;
            } catch (IOException e) {
                LOGGER.error("failed to load cid", e);
            }
        }
        return null;
    }

    private static void writeCID(String path, String cid) {
        ObjectMapper om = new ObjectMapper();
        ObjectNode tree = om.createObjectNode();
        tree.put("cid", cid);
        try {
            om.writeValue(new File(path), tree);
        } catch (IOException e) {
            LOGGER.error("failed to write cid", e);
        }
    }

    private synchronized void insertEvent(String[][] event) {
        long now = System.currentTimeMillis();
        if (now - lastSend > MIN_SEND_INTERVAL) {
            lastSend = now;
            if (events.size() < MAX_EVENTS) {
                events.add(event);
            } else {
                LOGGER.warn("too many events in log ({})", events.size());
            }
        }
    }

    public void trackEvent(String category, String action, String label) {
        String[][] event = new String[][] {
                {"v", "1"},
                {"tid", tid},
                {"cid", cid},
                {"t", "event"},
                {"ec", category},
                {"ea", action},
                {"el", label},
        };

        insertEvent(event);
    }

    public void trackScreen(String application, String screenName) {
        String[][] event = new String[][] {
                {"v", "1"},
                {"tid", tid},
                {"cid", cid},
                {"t", "screenview"},
                {"an", application},
                {"av", applicationVersion},
                {"cd", screenName}
        };

        insertEvent(event);
    }

    private static String encode(String value)  {
        try {
            return URLEncoder.encode(value, StandardCharsets.UTF_8.name());
        } catch (UnsupportedEncodingException e) {
            throw new RuntimeException(e);
        }
    }

    public void dispose() {
        // NOTE: Not a clean shutdown as we might still have a
        // pending http request when exiting this method. In practise its
        // probably fine however.
        running = false;
        this.interrupt();
    }

    @Override
    public void run() {
        while (running) {
            try {
                Thread.sleep(SEND_INTERVAL);

                try {
                    send();
                } catch (Throwable e) {

                }

            } catch (InterruptedException e) {}
        }
    }

    private synchronized List<String[][]> getBatch() {
        List<String[][]> b = new ArrayList<>();

        int count = Math.min(MAX_BATCH, events.size());
        for (int i = 0; i < count; i++) {
            b.add(events.remove(0));
        }
        return b;
    }

    private synchronized void reinsertBatch(List<String[][]> batch) {
        int count = batch.size();
        for (int i = 0; i < count; ++i) {
            events.add(0, batch.remove(batch.size() - 1));
        }
    }

    private static String payloadify(List<String[][]> batch) {
        StringBuffer payload = new StringBuffer();

        for (int i = 0; i < batch.size(); i++) {
            String[][] event = batch.get(i);

            for (int j = 0; j < event.length; j++) {
                payload.append(event[j][0]);
                payload.append("=");
                payload.append(encode(event[j][1]));
                if (j < event.length - 1) {
                    payload.append("&");
                }
            }
            payload.append("\n");
        }

        return payload.toString();
    }

    private void send() {
        List<String[][]> b = getBatch();
        if (b.size() == 0) {
            return;
        }

        String payload = payloadify(b);

        try {
            URL url = new URL(GA_URL);
            URLConnection con = url.openConnection();
            HttpURLConnection http = (HttpURLConnection)con;

            http.setRequestMethod("POST");
            http.setDoOutput(true);
            http.setFixedLengthStreamingMode(payload.length());
            http.setRequestProperty("Content-Type", "application/x-www-form-urlencoded; charset=UTF-8");
            http.connect();
            try (OutputStream os = http.getOutputStream()) {
                os.write(payload.getBytes(StandardCharsets.UTF_8));
            }
            int code = http.getResponseCode();
            if (code == 200) {
                // OK
                return;
            }


        } catch (IOException e) {
            // NOTE: We don't log on errors as we don't have backoff inplace
            // Same for code != 200 above
        }
        reinsertBatch(b);
        return;
    }

}
