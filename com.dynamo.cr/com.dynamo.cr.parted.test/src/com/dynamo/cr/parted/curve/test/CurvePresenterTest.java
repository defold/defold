package com.dynamo.cr.parted.curve.test;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Point2d;
import javax.vecmath.Vector2d;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.TreePath;
import org.eclipse.jface.viewers.TreeSelection;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.editor.core.operations.MergeableDelegatingOperationHistory;
import com.dynamo.cr.parted.curve.CurvePresenter;
import com.dynamo.cr.parted.curve.HermiteSpline;
import com.dynamo.cr.parted.curve.ICurveProvider;
import com.dynamo.cr.parted.curve.ICurveView;
import com.dynamo.cr.parted.curve.SplinePoint;
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

    private static final Vector2d SCREEN_SCALE = new Vector2d(500.0, -200.0);
    private static final double SCREEN_DRAG_PADDING = 2;
    private static final double SCREEN_HIT_PADDING = 4;
    private static final double SCREEN_TANGENT_LENGTH = 50;

    private class HistoryListener implements IOperationHistoryListener {
        public int doneCount = 0;
        public int changedCount = 0;
        @Override
        public void historyNotification(OperationHistoryEvent event) {
            switch (event.getEventType()) {
            case OperationHistoryEvent.OPERATION_CHANGED:
                ++changedCount;
                break;
            case OperationHistoryEvent.DONE:
                ++doneCount;
                break;
            }
        }
    }

    private ICurveView view;
    private ICurveProvider curveProvider;
    private CurvePresenter presenter;
    private DummyNode node;
    private IPropertyModel<Node, IPropertyObjectWorld> model;
    private IOperationHistory history;
    private HistoryListener historyListener;
    private IUndoContext undoContext;
    private int commandDoneCount;
    private int commandChangedCount;

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
        this.curveProvider = mock(ICurveProvider.class);
        when(this.curveProvider.isEnabled(anyInt())).thenReturn(true);
        this.history = new MergeableDelegatingOperationHistory(new DefaultOperationHistory());
        this.undoContext = new UndoContext();
        this.historyListener = new HistoryListener();
        this.history.addOperationHistoryListener(this.historyListener);

        Injector injector = Guice.createInjector(new Module());

        this.presenter = (CurvePresenter)injector.getInstance(ICurveView.IPresenter.class);
        this.presenter.setCurveProvider(this.curveProvider);
        this.node = new DummyNode();
        this.model = (IPropertyModel<Node, IPropertyObjectWorld>)this.node.getAdapter(IPropertyModel.class);
        this.presenter.setModel(model);
        this.commandDoneCount = 0;
        this.commandChangedCount = 0;
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
        this.presenter.setSelection(new TreeSelection(paths.toArray(new TreePath[points.length])));
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

    private void selectNone() {
        select(new int[][] {});
    }

    private void hideCurve(int curveIndex) {
        when(this.curveProvider.isEnabled(curveIndex)).thenReturn(false);
    }

    private void verifyCommandDone(Class<? extends IUndoableOperation> operationClass) {
        ++this.commandDoneCount;
        assertThat(this.historyListener.doneCount, is(this.commandDoneCount));
        assertThat(this.history.getUndoOperation(this.undoContext), instanceOf(operationClass));
    }

    private void verifyCommandChanged(Class<? extends IUndoableOperation> operationClass) {
        ++this.commandChangedCount;
        assertThat(this.historyListener.changedCount, is(this.commandChangedCount));
        assertThat(this.history.getUndoOperation(this.undoContext), instanceOf(operationClass));
    }

    private void verifyNoExecution() {
        assertThat(this.historyListener.doneCount, is(this.commandDoneCount));
        assertThat(this.historyListener.changedCount, is(this.commandChangedCount));
    }

    private void verifySelection(int[][] selectedIndices) {
        List<TreePath> selectedPaths = new ArrayList<TreePath>(selectedIndices.length);
        for (int i = 0; i < selectedIndices.length; ++i) {
            int[] indices = selectedIndices[i];
            Integer[] intIndices = new Integer[indices.length];
            for (int j = 0; j < indices.length; ++j) {
                intIndices[j] = indices[j];
            }
            selectedPaths.add(new TreePath(intIndices));
        }
        ISelection selection = new TreeSelection(selectedPaths.toArray(new TreePath[selectedIndices.length]));
        assertThat(this.presenter.getSelection(), equalTo(selection));
    }

    private void verifyNoSelection() {
        verifySelection(new int[][] {});
    }

    private void undo() throws ExecutionException {
        this.history.undo(this.undoContext, null, null);
    }

    @Test
    public void testAddPoint() throws ExecutionException {
        selectCurve(0);
        Point2d p = new Point2d(0.5, 0.5);
        this.presenter.onAddPoint(p);
        verifyCommandDone(InsertPointOperation.class);
        verifySelection(new int[][] {{0, 1}});
        undo();
        verifySelection(new int[][] {{0}});
    }

    @Test
    public void testAddPointDoubleClick() {
        // Simulate double click
        Point2d p = new Point2d(0.1, 0.1);
        this.presenter.onStartDrag(p, SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        this.presenter.onEndDrag();
        this.presenter.onStartDrag(p, SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifySelection(new int[][] {{0}});
        this.presenter.onAddPoint(p);
        verifyCommandDone(InsertPointOperation.class);
    }

    @Test
    public void testAddPointXSubZero() throws ExecutionException {
        selectCurve(0);
        Point2d p = new Point2d(-0.1, 0.5);
        this.presenter.onAddPoint(p);
        verifyNoExecution();
        verifySelection(new int[][] {{0, 0}});
    }

    @Test
    public void testAddPointXAboveOne() throws ExecutionException {
        selectCurve(0);
        Point2d p = new Point2d(1.1, 0.5);
        this.presenter.onAddPoint(p);
        verifyNoExecution();
        verifySelection(new int[][] {{0, 1}});
    }

    @Test
    public void testAddPointTwice() throws ExecutionException {
        selectCurve(0);
        Point2d p = new Point2d(0.5, 0.5);
        this.presenter.onAddPoint(p);
        verifyCommandDone(InsertPointOperation.class);
        verifySelection(new int[][] {{0, 1}});
        this.presenter.onAddPoint(p);
        verifyNoExecution();
        verifySelection(new int[][] {{0, 1}});
    }

    @Test
    public void testRemoveAddedPoint() throws ExecutionException {
        selectCurve(0);
        Point2d p = new Point2d(0.5, 0.5);
        this.presenter.onAddPoint(p);
        verifyCommandDone(InsertPointOperation.class);
        this.presenter.onRemove();
        verifyCommandDone(RemovePointsOperation.class);
        verifySelection(new int[][] {{0}});
        undo();
        verifySelection(new int[][] {{0, 1}});
    }

    @Test
    public void testRemoveMultiplePoints() {
        selectCurve(0);
        Point2d p = new Point2d(0.5, 0.5);
        this.presenter.onAddPoint(p);
        verifyCommandDone(InsertPointOperation.class);
        select(new int[][] {{0, 0}, {0, 1}, {0, 2}});
        this.presenter.onRemove();
        verifyCommandDone(RemovePointsOperation.class);
        verifySelection(new int[][] {{0, 0}, {0, 1}});
    }

    @Test
    public void testRemovePointDoubleClick() {
        selectCurve(0);
        Point2d p = new Point2d(0.1, 0.1);
        this.presenter.onAddPoint(p);
        verifyCommandDone(InsertPointOperation.class);
        // Simulate double click
        this.presenter.onStartDrag(p, SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        this.presenter.onEndDrag();
        this.presenter.onStartDrag(p, SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifySelection(new int[][] {{0, 1}});
        this.presenter.onRemove();
        verifyCommandDone(RemovePointsOperation.class);
        verifySelection(new int[][] {{0}});
    }

    @Test
    public void testMovePoints() {
        selectCurveAllPoints(0);
        this.presenter.onStartDrag(new Point2d(0.0, 0.0), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifyNoExecution();
        this.presenter.onDrag(new Point2d(0.0, 1.0));
        verifyCommandDone(MovePointsOperation.class);
        this.presenter.onDrag(new Point2d(0.0, 2.0));
        verifyCommandChanged(MovePointsOperation.class);
        this.presenter.onEndDrag();
        verifyCommandChanged(MovePointsOperation.class);
        HermiteSpline spline = getCurve(0);
        assertThat(spline.getPoint(0).getY(), is(2.0));
        assertThat(spline.getPoint(1).getY(), is(3.0));
    }

    @Test
    public void testEmptyMovePoints() {
        selectCurveAllPoints(0);
        this.presenter.onStartDrag(new Point2d(0.0, 0.0), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifyNoExecution();
        this.presenter.onDrag(new Point2d(0.0, 0.0));
        verifyNoExecution();
        this.presenter.onEndDrag();
        verifyNoExecution();
    }

    @Test
    public void testMoveNoPoints() {
        selectCurve(0);
        this.presenter.onStartDrag(new Point2d(0.0, 0.5), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifyNoExecution();
        this.presenter.onDrag(new Point2d(0.0, 1.0));
        verifyNoExecution();
        this.presenter.onEndDrag();
        verifyNoExecution();
    }

    @Test
    public void testMovePointsMultipleCurves() {
        select(new int[][] {{0, 0}, {1, 0}});
        this.presenter.onStartDrag(new Point2d(0.0, 0.0), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifyNoExecution();
        this.presenter.onDrag(new Point2d(0.0, 1.0));
        verifyCommandDone(MovePointsOperation.class);
        this.presenter.onEndDrag();
        verifyCommandChanged(MovePointsOperation.class);
        assertThat(getCurve(0).getPoint(0).getY(), is(1.0));
        assertThat(getCurve(1).getPoint(0).getY(), is(2.5));
    }

    @Test
    public void testMoveTangent() {
        selectCurve(0);
        Vector2d tangent = new Vector2d(1.0, 1.0);
        tangent.normalize();
        tangent.set(tangent.getX() * SCREEN_SCALE.getX(), tangent.getY() * SCREEN_SCALE.getY());
        tangent.normalize();
        tangent.scale(SCREEN_TANGENT_LENGTH);
        tangent.set(tangent.getX() / SCREEN_SCALE.getX(), tangent.getY() / SCREEN_SCALE.getY());
        this.presenter.onStartDrag(new Point2d(tangent), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifyNoExecution();
        this.presenter.onDrag(new Point2d(1.0, 1.0));
        verifyCommandDone(SetTangentOperation.class);
        this.presenter.onDrag(new Point2d(1.0, 0.0));
        verifyCommandChanged(SetTangentOperation.class);
        this.presenter.onEndDrag();
        verifyCommandChanged(SetTangentOperation.class);
    }

    @Test
    public void testEmptyMoveTangent() {
        selectCurve(0);
        Vector2d tangent = new Vector2d(1.0, 1.0);
        tangent.normalize();
        tangent.set(tangent.getX() * SCREEN_SCALE.getX(), tangent.getY() * SCREEN_SCALE.getY());
        tangent.normalize();
        tangent.scale(SCREEN_TANGENT_LENGTH);
        tangent.set(tangent.getX() / SCREEN_SCALE.getX(), tangent.getY() / SCREEN_SCALE.getY());
        this.presenter.onStartDrag(new Point2d(tangent), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifyNoExecution();
        this.presenter.onDrag(new Point2d(tangent));
        verifyNoExecution();
        this.presenter.onEndDrag();
        verifyNoExecution();
    }

    @Test
    public void testMoveTangentLimit() {
        selectCurve(0);
        Vector2d tangent = new Vector2d(1.0, 1.0);
        tangent.normalize();
        tangent.set(tangent.getX() * SCREEN_SCALE.getX(), tangent.getY() * SCREEN_SCALE.getY());
        tangent.normalize();
        tangent.scale(SCREEN_TANGENT_LENGTH);
        tangent.set(tangent.getX() / SCREEN_SCALE.getX(), tangent.getY() / SCREEN_SCALE.getY());
        this.presenter.onStartDrag(new Point2d(tangent), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifyNoExecution();
        this.presenter.onDrag(new Point2d(0.0, 1.0));
        verifyCommandDone(SetTangentOperation.class);
        this.presenter.onEndDrag();
        verifyCommandChanged(SetTangentOperation.class);
        HermiteSpline spline = getCurve(0);
        SplinePoint point = spline.getPoint(0);
        assertTrue(point.getTx() > 0.0);
        assertTrue(Math.abs(point.getTy()) < 1.0);
    }

    @Test
    public void testSelectClick() {
        this.presenter.onStartDrag(new Point2d(0.0, 0.0), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifySelection(new int[][] {{0, 0}});
        this.presenter.onEndDrag();
        verifySelection(new int[][] {{0, 0}});
    }

    @Test
    public void testSelectClickHidden() {
        hideCurve(0);
        this.presenter.onStartDrag(new Point2d(0.0, 0.0), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifyNoSelection();
        this.presenter.onEndDrag();
        verifyNoSelection();
    }

    @Test
    public void testSelectClickSecond() {
        this.presenter.onStartDrag(new Point2d(0.0, 1.5), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifySelection(new int[][] {{1, 0}});
        this.presenter.onEndDrag();
        verifySelection(new int[][] {{1, 0}});
    }

    @Test
    public void testSelectBox() {
        this.presenter.onStartDrag(new Point2d(-1.0, -1.0), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifyNoSelection();
        this.presenter.onDrag(new Point2d(1.0, 1.0));
        verifySelection(new int[][] {{0, 0}, {0, 1}});
        this.presenter.onEndDrag();
        verifySelection(new int[][] {{0, 0}, {0, 1}});
    }

    @Test
    public void testSelectBoxHidden() {
        hideCurve(0);
        this.presenter.onStartDrag(new Point2d(-1.0, -1.0), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifyNoSelection();
        this.presenter.onDrag(new Point2d(1.0, 1.0));
        verifyNoSelection();
        this.presenter.onEndDrag();
        verifyNoSelection();
    }

    @Test
    public void testEmptySelection() {
        this.presenter.onStartDrag(new Point2d(0.0, 1.0), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifyNoSelection();
        this.presenter.onEndDrag();
        verifyNoSelection();
    }

    @Test
    public void testSelectAll() {
        selectCurve(0);
        Point2d p = new Point2d(0.5, 0.5);
        this.presenter.onAddPoint(p);
        verifyCommandDone(InsertPointOperation.class);
        this.presenter.onSelectAll();
        verifySelection(new int[][] {{0, 0}, {0, 1}, {0, 2}, {1, 0}, {1, 1}});
    }

    @Test
    public void testSelectAllHidden() {
        hideCurve(0);
        hideCurve(1);
        this.presenter.onSelectAll();
        verifyNoSelection();
    }

    @Test
    public void testSelectionOverlayedPoints() {
        // Move first point on top of first point of second curve
        select(new int[][] {{0, 0}});
        this.presenter.onStartDrag(new Point2d(0.0, 0.0), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        this.presenter.onDrag(new Point2d(0.0, 1.5));
        this.presenter.onEndDrag();

        // deselect
        selectNone();

        // Click first point of second curve
        this.presenter.onStartDrag(new Point2d(0.0, 1.5), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        this.presenter.onEndDrag();

        verifySelection(new int[][] {{0, 0}});
    }

    @Test
    public void testSelectionOverlayedPointsSelectedCurve() {
        // Move first point on top of first point of second curve
        select(new int[][] {{0, 0}});
        this.presenter.onStartDrag(new Point2d(0.0, 0.0), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        this.presenter.onDrag(new Point2d(0.0, 1.5));
        this.presenter.onEndDrag();

        // Select second curve
        selectCurve(1);

        // Click first point of second curve
        this.presenter.onStartDrag(new Point2d(0.0, 1.5), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        this.presenter.onEndDrag();

        verifySelection(new int[][] {{1, 0}});
    }

    @Test
    public void testSelectCurve() {
        this.presenter.onStartDrag(new Point2d(0.2, 0.2), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifySelection(new int[][] {{0}});
        this.presenter.onEndDrag();
        verifySelection(new int[][] {{0}});
    }

    @Test
    public void testSelectPointFromUnselected() {
        selectCurve(1);
        this.presenter.onStartDrag(new Point2d(0.0, 0.0), SCREEN_SCALE, SCREEN_DRAG_PADDING, SCREEN_HIT_PADDING, SCREEN_TANGENT_LENGTH);
        verifySelection(new int[][] {{0, 0}});
        this.presenter.onEndDrag();
        verifySelection(new int[][] {{0, 0}});
    }

    @Test
    public void testPickRetainSelectionPoint() {
        selectCurveAllPoints(0);
        this.presenter.onPickSelect(new Point2d(0.0, 0.0), SCREEN_SCALE, SCREEN_HIT_PADDING);
        verifySelection(new int[][] {{0, 0}, {0, 1}});
    }

    @Test
    public void testPickRetainSelectionCurve() {
        selectCurveAllPoints(0);
        this.presenter.onPickSelect(new Point2d(0.5, 0.5), SCREEN_SCALE, SCREEN_HIT_PADDING);
        verifySelection(new int[][] {{0, 0}, {0, 1}});
    }

    @Test
    public void testPickRetainSelectionEmpty() {
        selectCurveAllPoints(0);
        this.presenter.onPickSelect(new Point2d(0.0, 0.5), SCREEN_SCALE, SCREEN_HIT_PADDING);
        verifySelection(new int[][] {{0, 0}, {0, 1}});
    }

    @Test
    public void testPickPoint() {
        select(new int[][] {{0, 1}});
        this.presenter.onPickSelect(new Point2d(0.0, 0.0), SCREEN_SCALE, SCREEN_HIT_PADDING);
        verifySelection(new int[][] {{0, 0}});
    }

    @Test
    public void testPickPointHidden() {
        hideCurve(0);
        this.presenter.onPickSelect(new Point2d(0.0, 0.0), SCREEN_SCALE, SCREEN_HIT_PADDING);
        verifyNoSelection();
    }

    @Test
    public void testPickCurve() {
        selectCurveAllPoints(1);
        this.presenter.onPickSelect(new Point2d(0.2, 0.2), SCREEN_SCALE, SCREEN_HIT_PADDING);
        verifySelection(new int[][] {{0}});
    }

    @Test
    public void testPickCurveHidden() {
        hideCurve(0);
        this.presenter.onPickSelect(new Point2d(0.2, 0.2), SCREEN_SCALE, SCREEN_HIT_PADDING);
        verifyNoSelection();
    }
}
