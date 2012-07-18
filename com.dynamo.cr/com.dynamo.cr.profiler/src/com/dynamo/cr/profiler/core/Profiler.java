package com.dynamo.cr.profiler.core;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.ws.rs.core.UriBuilder;

import org.apache.commons.io.IOUtils;

import com.dynamo.cr.profiler.core.ProfileData.Frame;

public class Profiler {

    private List<ByteBuffer> capturedFrames = new ArrayList<ByteBuffer>();
    private Map<Long, String> stringMap = new HashMap<Long, String>();
    private ByteBuffer stringBuffer;

    public void captureFrame(String url) throws IOException {

        URI profileUri = null;
        try {
            profileUri = UriBuilder.fromUri(url).path("profile").port(8002).build();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        HttpURLConnection conn = (HttpURLConnection) profileUri.toURL().openConnection();
        conn.setRequestMethod("GET");
        conn.connect();

        InputStream is = conn.getInputStream();
        try {
            ByteArrayOutputStream bos = new ByteArrayOutputStream(1024 * 1024);

            byte[] buf = new byte[256 * 1024];
            int n = is.read(buf);
            while (n > 0) {
                bos.write(buf, 0, n);
                n = is.read(buf);
            }
            bos.close();
            ByteBuffer buffer = ByteBuffer.wrap(bos.toByteArray());
            buffer.order(ByteOrder.LITTLE_ENDIAN);
            capturedFrames.add(buffer);
        } finally {
            IOUtils.closeQuietly(is);
        }
    }

    public void captureStrings(String url) throws IOException {
        URI profileUri = null;
        try {
            profileUri = UriBuilder.fromUri(url).path("strings").port(8002).build();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        HttpURLConnection conn = (HttpURLConnection) profileUri.toURL().openConnection();
        conn.setRequestMethod("GET");
        conn.connect();

        InputStream is = conn.getInputStream();
        try {
            ByteArrayOutputStream bos = new ByteArrayOutputStream(1024 * 1024);

            byte[] buf = new byte[256 * 1024];
            int n = is.read(buf);
            while (n > 0) {
                bos.write(buf, 0, n);
                n = is.read(buf);
            }
            bos.close();
            stringBuffer  = ByteBuffer.wrap(bos.toByteArray());
            stringBuffer.order(ByteOrder.LITTLE_ENDIAN);

        } finally {
            IOUtils.closeQuietly(is);
        }
    }

    private void parseStrings() throws ProfilerException {
        stringBuffer.rewind();

        byte[] fourCC = new byte[4];
        stringBuffer.get(fourCC);
        if (!Arrays.equals(fourCC, "STRS".getBytes())) {
            throw new ProfilerException("Malformed profile response (exepcted STRS)");
        }

        long nStrings = stringBuffer.getInt() & 0xffffffff;

        stringMap.clear();
        for (int i = 0; i < nStrings; ++i) {
            long id = stringBuffer.getInt() & 0xffffffff;
            int len = stringBuffer.getShort() & 0xffff;
            if (len == 0) {
                stringMap.put(id, "");
            } else {
                byte[] strBuf = new byte[len];
                stringBuffer.get(strBuf);
                String str = new String(strBuf);
                stringMap.put(id, str);
            }
        }
    }

    private String getString(long id) {
        String s = stringMap.get(id);

        if (s == null) {
            s = String.format("unknown (%d)", id);
        }
        return s;
    }

    private ProfileData.Sample[] parseSamples(ByteBuffer buffer) {
        int nSamples = buffer.getInt();
        ProfileData.Sample[] samples = new ProfileData.Sample[nSamples];
        for (int i = 0; i < nSamples; ++i) {
            long nameId = buffer.getInt() & 0xffffffff;
            long scopeId = buffer.getInt() & 0xffffffff;
            String name = getString(nameId);
            String scope = getString(scopeId);

            long start = buffer.getInt() & 0xffffffff;
            long elapsed = buffer.getInt() & 0xffffffff;
            int threadId = buffer.getShort() & 0xffff;
            /* short pad = */ buffer.getShort();
            ProfileData.Sample sample = new ProfileData.Sample(name, scope, start, elapsed, threadId);
            samples[i] = sample;
        }
        return samples;
    }

    private ProfileData.Scope[] parseScopes(ByteBuffer buffer) {
        int nScopes = buffer.getInt();
        ProfileData.Scope[] scopes = new ProfileData.Scope[nScopes];
        for (int i = 0; i < nScopes; ++i) {
            long scopeId = buffer.getInt() & 0xffffffff;
            String scopeName = getString(scopeId);

            long elapsed = buffer.getInt() & 0xffffffff;
            long count = buffer.getInt() & 0xffff;
            ProfileData.Scope scope = new ProfileData.Scope(scopeName, elapsed, count);
            scopes[i] = scope;
        }
        return scopes;
    }

    private ProfileData.Counter[] parseCounters(ByteBuffer buffer) {
        int nCounters = buffer.getInt();
        ProfileData.Counter[] counters = new ProfileData.Counter[nCounters];
        for (int i = 0; i < nCounters; ++i) {
            long counterId = buffer.getInt() & 0xffffffff;
            String counterName = getString(counterId);
            long value = buffer.getInt() & 0xffffffff;
            ProfileData.Counter counter = new ProfileData.Counter(counterName, value);
            counters[i] = counter;
        }
        return counters;
    }

    private ProfileData.Frame parseFrame(ByteBuffer buffer) throws ProfilerException {

        byte[] fourCC = new byte[4];
        buffer.get(fourCC);
        if (!Arrays.equals(fourCC, "PROF".getBytes())) {
            throw new ProfilerException("Malformed profile response (exepcted PROF)");
        }

        ProfileData.Sample[] samples = parseSamples(buffer);
        ProfileData.Scope[] scopes = parseScopes(buffer);
        ProfileData.Counter[] counters = parseCounters(buffer);

        return new ProfileData.Frame(samples, scopes, counters);
    }

    public ProfileData parse() throws ProfilerException {
        parseStrings();
        Frame[] frames = new Frame[capturedFrames.size()];
        int i = 0;
        for (ByteBuffer buffer : capturedFrames) {
            Frame frame = parseFrame(buffer);
            frames[i++] = frame;
        }
        return new ProfileData(frames);
    }
}
