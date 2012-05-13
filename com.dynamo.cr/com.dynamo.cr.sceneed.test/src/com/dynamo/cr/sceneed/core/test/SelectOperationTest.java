package com.dynamo.cr.sceneed.core.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;
import org.junit.Test;

import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.AbstractSelectOperation;

public class SelectOperationTest {

    private static class DummyOperation extends AbstractSelectOperation {

        public int testValue = 0;

        public DummyOperation(Node selected, IPresenterContext presenterContext) {
            super("Select Node", selected, presenterContext);
        }

        @Override
        protected IStatus doExecute(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
            testValue = 1;
            return Status.OK_STATUS;
        }

        @Override
        protected IStatus doRedo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
            testValue = 2;
            return Status.OK_STATUS;
        }

        @Override
        protected IStatus doUndo(IProgressMonitor monitor, IAdaptable info) throws ExecutionException {
            testValue = 0;
            return Status.OK_STATUS;
        }

    }

    @Test
    public void test() throws Exception {
        IPresenterContext context = mock(IPresenterContext.class);
        when(context.getSelection()).thenReturn(new StructuredSelection());

        DummyOperation op = new DummyOperation(new DummyNode(), context);

        op.execute(null, null);
        assertThat(op.testValue, is(1));
        verify(context, times(1)).setSelection(any(IStructuredSelection.class));

        op.undo(null, null);
        assertThat(op.testValue, is(0));
        verify(context, times(2)).setSelection(any(IStructuredSelection.class));

        op.redo(null, null);
        assertThat(op.testValue, is(2));
        verify(context, times(3)).setSelection(any(IStructuredSelection.class));
    }
}
