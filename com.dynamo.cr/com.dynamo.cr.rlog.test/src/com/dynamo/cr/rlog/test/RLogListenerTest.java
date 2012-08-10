package com.dynamo.cr.rlog.test;

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.*;

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
    public void testRetain() {
        when(transport.send(any(Record.class))).thenReturn(true);
        log(RLogListener.RETAIN_COUNT + 1);
        listener.process();
        verify(transport, times(RLogListener.RETAIN_COUNT)).send(any(Record.class));
    }

    @Test
    public void testAlwaysFail() {
        int n = 10;
        when(transport.send(any(Record.class))).thenReturn(false);
        log(n);
        listener.process();
        verify(transport, times(1)).send(any(Record.class));
    }

    @Test
    public void testTransientFail() {
        int n = 10;
        when(transport.send(any(Record.class))).thenReturn(false).thenReturn(true);
        log(n);
        listener.process();
        verify(transport, times(1)).send(any(Record.class));
        listener.process();
        verify(transport, times(n+1)).send(any(Record.class));
    }

    @Test
    public void testTransientTransportException() {
        int n = 10;
        when(transport.send(any(Record.class))).thenThrow(new NullPointerException()).thenReturn(true);
        log(n);
        System.err.println("Expected stack-trace printed after this:");
        listener.process();
        verify(transport, times(1)).send(any(Record.class));
        listener.process();
        verify(transport, times(n+1)).send(any(Record.class));
    }

    private IStatus newStatus() {
        return new Status(IStatus.ERROR, "test", "an error");
    }

}
