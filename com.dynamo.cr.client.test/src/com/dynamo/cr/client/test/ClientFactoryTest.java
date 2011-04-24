package com.dynamo.cr.client.test;

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import java.net.URI;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.client.DelegatingClientFactory;
import com.dynamo.cr.client.IClientFactory;

public class ClientFactoryTest {

    @Before
    public void setup() {

    }

    @Test
    public void getProjectClientCaching() throws Exception {
        IClientFactory factory = mock(IClientFactory.class);
        DelegatingClientFactory delegatingFactory = spy(new DelegatingClientFactory(factory));

        URI uri1 = new URI("crepo://host:1234/project/my_branch");
        // NOTE: Test that normalization/canonicalization works
        URI uri2 = new URI("crepo://host:1234//project//my_branch/");
        delegatingFactory.getProjectClient(uri1);
        delegatingFactory.getProjectClient(uri2);

        verify(delegatingFactory, times(2)).getProjectClient((URI) any());
        verify(factory, times(1)).getProjectClient((URI) any());
    }

    @Test
    public void getProjectsClientCaching() throws Exception {
        IClientFactory factory = mock(IClientFactory.class);
        DelegatingClientFactory delegatingFactory = spy(new DelegatingClientFactory(factory));

        URI uri1 = new URI("crepo://host:1234/project/my_branch");
        // NOTE: Test that normalization/canonicalization works
        URI uri2 = new URI("crepo://host:1234//project//my_branch/");
        delegatingFactory.getProjectsClient(uri1);
        delegatingFactory.getProjectsClient(uri2);

        verify(delegatingFactory, times(2)).getProjectsClient((URI) any());
        verify(factory, times(1)).getProjectsClient((URI) any());
    }

    @Test
    public void getBranchClientCaching() throws Exception {
        IClientFactory factory = mock(IClientFactory.class);
        DelegatingClientFactory delegatingFactory = spy(new DelegatingClientFactory(factory));

        URI uri1 = new URI("crepo://host:1234/project/my_branch");
        // NOTE: Test that normalization/canonicalization works
        URI uri2 = new URI("crepo://host:1234//project//my_branch/");
        delegatingFactory.getBranchClient(uri1);
        delegatingFactory.getBranchClient(uri2);

        verify(delegatingFactory, times(2)).getBranchClient((URI) any());
        verify(factory, times(1)).getBranchClient((URI) any());
    }

    @Test
    public void getUsersClientCaching() throws Exception {
        IClientFactory factory = mock(IClientFactory.class);
        DelegatingClientFactory delegatingFactory = spy(new DelegatingClientFactory(factory));

        URI uri1 = new URI("crepo://host:1234/project/my_branch");
        // NOTE: Test that normalization/canonicalization works
        URI uri2 = new URI("crepo://host:1234//project//my_branch/");
        delegatingFactory.getUsersClient(uri1);
        delegatingFactory.getUsersClient(uri2);

        verify(delegatingFactory, times(2)).getUsersClient((URI) any());
        verify(factory, times(1)).getUsersClient((URI) any());
    }

}
