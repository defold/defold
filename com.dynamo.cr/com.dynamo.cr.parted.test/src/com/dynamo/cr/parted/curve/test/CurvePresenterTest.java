package com.dynamo.cr.parted.curve.test;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Point2d;

import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionProvider;
import org.eclipse.jface.viewers.TreePath;
import org.eclipse.jface.viewers.TreeSelection;
import org.junit.Before;
import org.junit.Test;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import com.dynamo.cr.parted.curve.CurvePresenter;
import com.dynamo.cr.parted.curve.HermiteSpline;
import com.dynamo.cr.parted.curve.ICurveView;
import com.dynamo.cr.parted.operations.InsertPointOperation;
import com.dynamo.cr.parted.operations.MovePointsOperation;
import com.dynamo.cr.parted.operations.RemovePointsOperation;
import com.dynamo.cr.parted.operations.SetTangentOperation;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.Node;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Singleton;

public class CurvePresenterTest {

    private double PADDING = 0.1;

    private ICurveView view;
    private ISelectionProvider selectionProvider;
    private CurvePresenter presenter;
    private DummyNode node;
    private IPropertyModel<Node, IPropertyObjectWorld> model;
    private IOperationHistory history;
    private IUndoContext undoContext;
    private int executionCount;

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(ICurveView.class).toInstance(view);
            bind(ICurveView.IPresenter.class).to(CurvePresenter.class).in(Singleton.class);
            bind(IOperationHistory.class).toInstance(history);
            bind(IUndoContext.class).toInstance(undoContext);
        }
    }

    @SuppressWarnings("unchecked")
    @Before
    public void setup() {
        this.view = mock(ICurveView.class);
        this.selectionProvider = mock(ISelectionProvider.class);
        this.history = new DefaultOperationHistory();
        this.undoContext = new UndoContext();

        Injector injector = Guice.createInjector(new Module());

        this.presenter = (CurvePresenter)injector.getInstance(ICurveView.IPresenter.class);
        this.presenter.setSelectionProvider(this.selectionProvider);
        this.node = new DummyNode();
        this.model = (IPropertyModel<Node, IPropertyObjectWorld>)this.node.getAdapter(IPropertyModel.class);
        this.presenter.setModel(model);
        this.executionCount = 0;

        when(this.selectionProvider.getSelection()).thenReturn(new TreeSelection());
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                when(selectionProvider.getSelection()).thenReturn((ISelection)invocation.getArguments()[0]);
                return null;
            }
        }).when(this.selectionProvider).setSelection(any(ISelection.class));
    }

    private HermiteSpline getCurve(int curveIndex) {
        ValueSpread vs = (ValueSpread)this.model.getPropertyValue(presenter.getInput()[curveIndex].getId());
        return (HermiteSpline)vs.getCurve();
    }

    private void select(int[][] points) {
        List<TreePath> paths = new ArrayList<TreePath>(points.length);
        for (int i = 0; i < points.length; ++i) {
            Integer[] path = new Integer[points[i].length];
            for (int j = 0; j < path.length; ++j) {
                path[j] = points[i][j];
            }
            paths.add(new TreePath(path));
        }
        this.selectionProvider.setSelection(new TreeSelection(paths.toArray(new TreePath[points.length])));
    }

    private void selectCurve(int curveIndex) {
        select(new int[][] {{curveIndex}});
    }

    private void selectCurveAllPoints(int curveIndex) {
        HermiteSpline spline = getCurve(curveIndex);
        int pointCount = spline.getCount();
        int[][] points = new int[pointCount][];
        for (int i = 0; i < pointCount; ++i) {
            points[i] = new int[] {curveIndex, i};
        }
        select(points);
    }

    private void verifyExecution(Class<? extends IUndoableOperation> operationClass) {
        ++this.executionCount;
        IUndoableOperation[] undoHistory = this.history.getUndoHistory(this.undoContext);
        assertThat(undoHistory.length, is(this.executionCount));
        assertThat(undoHistory[this.executionCount-1], instanceOf(operationClass));
    }

    private void verifyNoExecution() {
        IUndoableOperation[] undoHistory = this.history.getUndoHistory(this.undoContext);
        assertThat(undoHistory.length, is(this.executionCount));
    }

    private void verifySelection(int[][] points) {
        List<TreePath> selectedPoints = new ArrayList<TreePath>(points.length);
        for (int i = 0; i < points.length; ++i) {
            selectedPoints.add(new TreePath(new Integer[] {points[i][0], points[i][1]}));
        }
        ISelection selection = new TreeSelection(selectedPoints.toArray(new TreePath[points.length]));
        assertThat(this.selectionProvider.getSelection(), equalTo(selection));
    }

    private void verifyNoSelection() {
        verifySelection(new int[][] {});
    }

    @Test
    public void testInsertPoint() {
        selectCurve(0);
        Point2d p = new Point2d(0.5, 0.5);
        this.presenter.onAddPoint(p);
        verifyExecution(InsertPointOperation.class);
        verifySelection(new int[][] {{0, 1}});
    }

    @Test
    public void testRemoveAddedPoint() {
        selectCurve(0);
        Point2d p = new Point2d(0.5, 0.5);
        this.presenter.onAddPoint(p);
        verifyExecution(InsertPointOperation.class);
        this.presenter.onRemove();
        verifyExecution(RemovePointsOperation.class);
        verifySelection(new int[][] {});
    }

    @Test
    public void testRemoveMultiplePoints() {
        selectCurve(0);
        Point2d p = new Point2d(0.5, 0.5);
        this.presenter.onAddPoint(p);
        verifyExecution(InsertPointOperation.class);
        select(new int[][] {{0, 0}, {0, 1}, {0, 2}});
        this.presenter.onRemove();
        verifyExecution(RemovePointsOperation.class);
        verifySelection(new int[][] {{0, 0}, {0, 1}});
    }

    @Test
    public void testMovePoints() {
        selectCurveAllPoints(0);
        this.presenter.onStartMove(new Point2d(0.0, 0.0));
        this.presenter.onMove(new Point2d(0.0, 1.0));
        verifyNoExecution();
        this.presenter.onMove(new Point2d(0.0, 1.0));
        verifyNoExecution();
        this.presenter.onEndMove();
        verifyExecution(MovePointsOperation.class);
    }

    @Test
    public void testEmptyMovePoints() {
        selectCurveAllPoints(0);
        this.presenter.onStartMove(new Point2d(0.0, 0.0));
        this.presenter.onMove(new Point2d(0.0, 0.0));
        verifyNoExecution();
        this.presenter.onEndMove();
        verifyNoExecution();
    }

    @Test
    public void testCancelMovePoints() {
        selectCurveAllPoints(0);
        this.presenter.onStartMove(new Point2d(0.0, 0.0));
        this.presenter.onMove(new Point2d(0.0, 1.0));
        verifyNoExecution();
        this.presenter.onCancelMove();
        verifyNoExecution();
    }

    @Test
    public void testMoveNoPoints() {
        selectCurve(0);
        this.presenter.onStartMove(new Point2d(0.0, 0.0));
        this.presenter.onMove(new Point2d(0.0, 1.0));
        verifyNoExecution();
        this.presenter.onMove(new Point2d(0.0, 1.0));
        verifyNoExecution();
        this.presenter.onEndMove();
        verifyNoExecution();
    }

    @Test
    public void testMoveTangent() {
        selectCurve(0);
        select(new int[][] {{0, 0}});
        this.presenter.onStartMoveTangent(new Point2d(0.0, 0.0));
        this.presenter.onMoveTangent(new Point2d(1.0, 1.0));
        verifyNoExecution();
        this.presenter.onMoveTangent(new Point2d(1.0, 0.0));
        verifyNoExecution();
        this.presenter.onEndMoveTangent();
        verifyExecution(SetTangentOperation.class);
    }

    @Test
    public void testEmptyMoveTangent() {
        selectCurve(0);
        select(new int[][] {{0, 0}});
        this.presenter.onStartMoveTangent(new Point2d(0.0, 0.0));
        this.presenter.onMoveTangent(new Point2d(0.0, 0.0));
        verifyNoExecution();
        this.presenter.onEndMoveTangent();
        verifyNoExecution();
    }

    @Test
    public void testCancelMoveTangent() {
        selectCurve(0);
        select(new int[][] {{0, 0}});
        this.presenter.onStartMoveTangent(new Point2d(0.0, 0.0));
        this.presenter.onMoveTangent(new Point2d(0.0, 1.0));
        verifyNoExecution();
        this.presenter.onCancelMoveTangent();
        verifyNoExecution();
    }

    @Test
    public void testSelectClick() {
        this.presenter.onStartSelect(new Point2d(0.0, 0.0), PADDING);
        verifySelection(new int[][] {{0, 0}});
        this.presenter.onEndSelect();
        verifySelection(new int[][] {{0, 0}});
    }

    @Test
    public void testSelectBox() {
        this.presenter.onStartSelect(new Point2d(0.0, 0.0), PADDING);
        verifySelection(new int[][] {{0, 0}});
        this.presenter.onSelect(new Point2d(1.0, 1.0));
        verifySelection(new int[][] {{0, 0}, {0, 1}});
        this.presenter.onEndSelect();
        verifySelection(new int[][] {{0, 0}, {0, 1}});
    }

    @Test
    public void testEmptySelection() {
        this.presenter.onStartSelect(new Point2d(0.0, 1.0), PADDING);
        verifyNoSelection();
        this.presenter.onEndSelect();
        verifyNoSelection();
    }

    @Test
    public void testSelectAll() {
        selectCurve(0);
        Point2d p = new Point2d(0.5, 0.5);
        this.presenter.onAddPoint(p);
        verifyExecution(InsertPointOperation.class);
        this.presenter.onSelectAll();
        verifySelection(new int[][] {{0, 0}, {0, 1}, {0, 2}, {1, 0}, {1, 1}});
    }

    @Test
    public void testSelectionOverlayedPoints() {
        // Move first point on top of first point of second curve
        select(new int[][] {{0, 0}});
        this.presenter.onStartMove(new Point2d(0.0, 0.0));
        this.presenter.onMove(new Point2d(0.0, 1.5));
        this.presenter.onEndMove();

        // Select second curve
        selectCurve(1);

        // Click first point of second curve
        this.presenter.onStartSelect(new Point2d(0.0, 1.5), PADDING);
        this.presenter.onEndSelect();

        verifySelection(new int[][] {{1, 0}});
    }

}
