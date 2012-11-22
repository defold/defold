package com.dynamo.cr.rlog.test;

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import java.io.IOException;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.rlog.IRLogTransport;
import com.dynamo.cr.rlog.RLogListener;
import com.dynamo.cr.rlog.proto.RLog.Record;

public class RLogListenerTest {

    private RLogListener listener;
    private IRLogTransport transport;

    @Before
    public void setUp() {
        transport = mock(IRLogTransport.class);
        listener = new RLogListener(transport);
    }

    @After
    public void tearDown() {
    }

    private void log(int n) {
        for (int i = 0; i < n; ++i) {
            listener.logging(newStatus(), "test");
        }
    }

    @Test
    public void testRetain() throws Exception {
        log(RLogListener.RETAIN_COUNT + 1);
        listener.process();
        verify(transport, times(RLogListener.RETAIN_COUNT)).send(any(Record.class));
    }

    @Test
    public void testThrottle() throws Exception {
        for (int i = 0; i < RLogListener.MAX_RECORDS + 1; ++i) {
            log(1);
            listener.process();
        }
        verify(transport, times(RLogListener.MAX_RECORDS)).send(any(Record.class));
    }

    @Test
    public void testPermanentAlwaysFail() throws Exception {
        int n = 10;
        doThrow(new NullPointerException()).when(transport).send(any(Record.class));
        log(n);
        for (int i = 0; i < n; ++i) {
            listener.process();
            verify(transport, times(i+1)).send(any(Record.class));
        }
        listener.process();
        verify(transport, times(n)).send(any(Record.class));
    }

    @Test
    public void testPermanentOnceFail() throws Exception {
        int n = 10;
        doThrow(new NullPointerException()).doNothing().when(transport).send(any(Record.class));
        log(n);
        listener.process();
        verify(transport, times(1)).send(any(Record.class));
        listener.process();
        // Only "n" due to permanent error (entry removed)
        verify(transport, times(n)).send(any(Record.class));
    }

    @Test
    public void testTransientOnceFail() throws Exception {
        int n = 10;
        doThrow(new IOException()).doNothing().when(transport).send(any(Record.class));
        log(n);
        listener.process();
        verify(transport, times(1)).send(any(Record.class));
        listener.process();
        // n + 1 due to retry
        verify(transport, times(n+1)).send(any(Record.class));
    }

    private IStatus newStatus() {
        return new Status(IStatus.ERROR, "test", "an error");
    }

}
