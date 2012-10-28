package com.dynamo.cr.parted.curve;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.vecmath.Point2d;
import javax.vecmath.Vector2d;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionProvider;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.ITreeSelection;
import org.eclipse.jface.viewers.TreePath;
import org.eclipse.jface.viewers.TreeSelection;

import com.dynamo.cr.parted.curve.ICurveView.IPresenter;
import com.dynamo.cr.parted.operations.InsertPointOperation;
import com.dynamo.cr.parted.operations.MovePointsOperation;
import com.dynamo.cr.parted.operations.RemovePointsOperation;
import com.dynamo.cr.parted.operations.SetTangentOperation;
import com.dynamo.cr.properties.IPropertyDesc;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.Node;
import com.google.inject.Inject;

public class CurvePresenter implements IPresenter {

    @Inject
    private ICurveView view;
    @Inject
    private IOperationHistory history;
    @Inject
    private IUndoContext undoContext;

    private ISelectionProvider selectionProvider;
    IPropertyModel<Node, IPropertyObjectWorld> propertyModel;
    @SuppressWarnings("unchecked")
    private IPropertyDesc<Node, ? extends IPropertyObjectWorld>[] input = new IPropertyDesc[0];
    @SuppressWarnings("unchecked")
    private IPropertyDesc<Node, ? extends IPropertyObjectWorld>[] oldInput = new IPropertyDesc[0];
    private MovePointsOperation moveOperation;
    private Point2d start = new Point2d();
    private SetTangentOperation setTangentOperation;
    private ISelection originalSelection;
    private double selectionPadding;

    public void setSelectionProvider(ISelectionProvider selectionProvider) {
        this.selectionProvider = selectionProvider;
    }

    public void setModel(IPropertyModel<Node, IPropertyObjectWorld> model) {
        this.propertyModel = model;
        refresh();
    }

    @SuppressWarnings("unchecked")
    public void refresh() {
        List<IPropertyDesc<Node, IPropertyObjectWorld>> lst = new ArrayList<IPropertyDesc<Node,IPropertyObjectWorld>>();
        if (this.propertyModel != null) {
            IPropertyDesc<Node, IPropertyObjectWorld>[] descs = this.propertyModel.getPropertyDescs();
            for (IPropertyDesc<Node, IPropertyObjectWorld> pd : descs) {
                Object value = this.propertyModel.getPropertyValue(pd.getId());
                if (value instanceof ValueSpread) {
                    ValueSpread vs = (ValueSpread) value;
                    if (vs.isAnimated()) {
                        lst.add(pd);
                    }
                }
            }
        }

        input = (IPropertyDesc<Node, IPropertyObjectWorld>[]) lst.toArray(new IPropertyDesc<?, ?>[lst.size()]);

        if (!Arrays.equals(input, oldInput)) {
            this.view.setInput(input);
        }

        oldInput = input;
        this.view.refresh();
    }

    public IPropertyDesc<Node, ? extends IPropertyObjectWorld>[] getInput() {
        return this.input;
    }

    private HermiteSpline getCurve(int index) {
        ValueSpread vs = (ValueSpread)this.propertyModel.getPropertyValue(this.input[index].getId());
        return (HermiteSpline)vs.getCurve();
    }

    private int getSingleCurveIndexFromSelection() {
        ISelection selection = this.selectionProvider.getSelection();
        if (!selection.isEmpty() && selection instanceof IStructuredSelection) {
            int index = -1;
            if (selection instanceof ITreeSelection) {
                ITreeSelection tree = (ITreeSelection)selection;
                for (TreePath path : tree.getPaths()) {
                    int i = (Integer)path.getSegment(0);
                    if (index == -1) {
                        index = i;
                    } else if (index != i) {
                        return -1;
                    }
                }
            }
            return index;
        }
        return -1;
    }

    private int[] getSinglePointIndexFromSelection() {
        ISelection selection = this.selectionProvider.getSelection();
        if (!selection.isEmpty() && selection instanceof IStructuredSelection) {
            if (selection instanceof ITreeSelection) {
                ITreeSelection tree = (ITreeSelection)selection;
                TreePath[] paths = tree.getPaths();
                if (paths.length == 1 && paths[0].getSegmentCount() == 2) {
                    return new int[] {(Integer)paths[0].getSegment(0), (Integer)paths[0].getSegment(1)};
                }
            }
        }
        return null;
    }

    private Map<Integer, List<Integer>> getPointsFromSelection() {
        Map<Integer, List<Integer>> points = new HashMap<Integer, List<Integer>>();
        ISelection selection = this.selectionProvider.getSelection();
        if (!selection.isEmpty() && selection instanceof IStructuredSelection) {
            if (selection instanceof ITreeSelection) {
                ITreeSelection tree = (ITreeSelection)selection;
                for (TreePath path : tree.getPaths()) {
                    int curveIndex = (Integer)path.getSegment(0);
                    if (path.getSegmentCount() > 1) {
                        int pointIndex = (Integer)path.getSegment(1);
                        if (points.containsKey(curveIndex)) {
                            points.get(curveIndex).add(pointIndex);
                        } else {
                            List<Integer> list = new ArrayList<Integer>();
                            list.add(pointIndex);
                            points.put(curveIndex, list);
                        }
                    }
                }
                // Sort point indices
                for (Map.Entry<Integer, List<Integer>> entry : points.entrySet()) {
                    Collections.sort(entry.getValue());
                }
            }
        }
        return points;
    }

    private IUndoableOperation setCurve(int index, HermiteSpline spline) {
        String id = this.input[index].getId();
        ValueSpread vs = (ValueSpread)this.propertyModel.getPropertyValue(id);
        vs.setCurve(spline);
        return this.propertyModel.setPropertyValue(this.input[index].getId(), vs, true);
    }

    private void execute(IUndoableOperation operation) {
        operation.addContext(this.undoContext);
        try {
            IStatus status = this.history.execute(operation, null, null);
            if (status != Status.OK_STATUS) {
                throw new RuntimeException("Failed to execute operation");
            }
        } catch (final ExecutionException e) {
            throw new RuntimeException("Failed to execute operation", e);
        }
    }

    private int[][] findPoints(Point2d min, Point2d max) {
        Vector2d delta = new Vector2d(max);
        delta.sub(min);
        List<int[]> points = new ArrayList<int[]>();
        // Select points
        for (int i = 0; i < this.input.length; ++i) {
            HermiteSpline spline = getCurve(i);
            for (int j = 0; j < spline.getCount(); ++j) {
                SplinePoint point = spline.getPoint(j);
                if (min.x <= point.getX() && point.getX() <= max.x
                        && min.y <= point.getY() && point.getY() <= max.y) {
                    points.add(new int[] {i, j});
                }
            }
        }
        return points.toArray(new int[points.size()][]);
    }

    private void select(int[][] points) {
        List<TreePath> selection = new ArrayList<TreePath>(points.length);
        for (int i = 0; i < points.length; ++i) {
            Integer[] value = new Integer[points[i].length];
            for (int j = 0; j < value.length; ++j) {
                value[j] = points[i][j];
            }
            selection.add(new TreePath(value));
        }
        this.selectionProvider.setSelection(new TreeSelection(selection.toArray(new TreePath[points.length])));
    }

    @Override
    public void onAddPoint(Point2d p) {
        int index = getSingleCurveIndexFromSelection();
        if (index >= 0) {
            HermiteSpline spline = getCurve(index);
            double x = p.x;
            spline = spline.insertPoint(p.x, p.y);
            execute(new InsertPointOperation(setCurve(index, spline)));
            int pointCount = spline.getCount();
            for (int i = 0; i < pointCount; ++i) {
                if (spline.getPoint(i).getX() == x) {
                    select(new int[][] {{index, i}});
                    break;
                }
            }
        }
    }

    @Override
    public void onRemove() {
        Map<Integer, List<Integer>> selectedPoints = getPointsFromSelection();
        if (!selectedPoints.isEmpty()) {
            List<int[]> newSelectedPoints = new ArrayList<int[]>();
            RemovePointsOperation removeOperation = new RemovePointsOperation();
            for (Map.Entry<Integer, List<Integer>> entry : selectedPoints.entrySet()) {
                int curveIndex = entry.getKey();
                HermiteSpline spline = getCurve(curveIndex);
                List<Integer> points = entry.getValue();
                Collections.sort(points);
                int pointCount = points.size();
                int removeCount = 0;
                for (int i = 0; i < pointCount; ++i) {
                    int pointIndex = points.get(i) - removeCount;
                    int preCount = spline.getCount();
                    spline = spline.removePoint(pointIndex);
                    if (preCount == spline.getCount()) {
                        newSelectedPoints.add(new int[] {curveIndex, pointIndex});
                    } else {
                        ++removeCount;
                    }
                }
                removeOperation.add(setCurve(curveIndex, spline));
            }
            execute(removeOperation);
            select(newSelectedPoints.toArray(new int[newSelectedPoints.size()][]));
        }
    }

    @Override
    public void onStartMove(Point2d start) {
        this.moveOperation = new MovePointsOperation(this.history);
        this.moveOperation.addContext(this.undoContext);
        this.moveOperation.begin();
        this.start.set(start);
    }

    @Override
    public void onMove(Point2d position) {
        Vector2d delta = new Vector2d();
        delta.sub(position, this.start);
        if (delta.lengthSquared() > 0.0) {
            if (this.moveOperation != null) {
                Map<Integer, List<Integer>> selectedPoints = getPointsFromSelection();
                for (Map.Entry<Integer, List<Integer>> entry : selectedPoints.entrySet()) {
                    HermiteSpline spline = getCurve(entry.getKey());
                    List<Integer> points = entry.getValue();
                    for (int pointIndex : points) {
                        SplinePoint point = spline.getPoint(pointIndex);
                        spline = spline.setPosition(pointIndex, point.getX() + delta.getX(), point.getY() + delta.getY());
                    }
                    execute(setCurve(entry.getKey(), spline));
                }
            } else {
                throw new IllegalStateException("No move has been started.");
            }
        }
    }

    @Override
    public void onEndMove() {
        if (this.moveOperation != null) {
            this.moveOperation.end(true);
        } else {
            throw new IllegalStateException("No move has been started.");
        }
    }

    @Override
    public void onCancelMove() {
        if (this.moveOperation != null) {
            this.moveOperation.end(false);
        } else {
            throw new IllegalStateException("No move has been started.");
        }
    }

    @Override
    public void onStartMoveTangent(Point2d start) {
        this.setTangentOperation = new SetTangentOperation(this.history);
        this.setTangentOperation.addContext(this.undoContext);
        this.setTangentOperation.begin();
    }

    @Override
    public void onMoveTangent(Point2d position) {
        int[] indices = getSinglePointIndexFromSelection();
        if (indices != null) {
            int curveIndex = indices[0];
            int pointIndex = indices[1];
            HermiteSpline spline = getCurve(curveIndex);
            SplinePoint point = spline.getPoint(pointIndex);
            Vector2d tangent = new Vector2d(point.getX(), point.getY());
            tangent.sub(position);
            if (tangent.lengthSquared() > 0.0) {
                if (tangent.getX() < 0.0) {
                    tangent.negate();
                }
                tangent.normalize();
                spline.setTangent(pointIndex, tangent.x, tangent.y);
                execute(setCurve(curveIndex, spline));
            }
        } else {
            throw new IllegalStateException("No single point selected.");
        }
    }

    @Override
    public void onEndMoveTangent() {
        if (this.setTangentOperation != null) {
            this.setTangentOperation.end(true);
        } else {
            throw new IllegalStateException("No move has been started.");
        }
    }

    @Override
    public void onCancelMoveTangent() {
        if (this.setTangentOperation != null) {
            this.setTangentOperation.end(false);
        } else {
            throw new IllegalStateException("No move has been started.");
        }
    }

    @Override
    public void onStartSelect(Point2d start, double padding) {
        this.start.set(start);
        this.originalSelection = this.selectionProvider.getSelection();
        this.selectionPadding = padding;
        Point2d min = new Point2d(-padding, -padding);
        min.add(start);
        Point2d max = new Point2d(padding, padding);
        max.add(start);
        int[][] points = findPoints(min, max);
        int curveIndex = getSingleCurveIndexFromSelection();
        if (curveIndex >= 0) {
            for (int i = 0; i < points.length; ++i) {
                if (points[i][0] == curveIndex) {
                    select(new int[][] {points[i]});
                }
            }
        } else {
            select(points);
        }
    }

    @Override
    public void onSelect(Point2d position) {
        if (this.originalSelection != null) {
            Vector2d padding = new Vector2d(this.selectionPadding, this.selectionPadding);
            Point2d min = new Point2d(Math.min(this.start.x, position.x), Math.min(this.start.y, position.y));
            min.sub(padding);
            Point2d max = new Point2d(Math.max(this.start.x, position.x), Math.max(this.start.y, position.y));
            max.add(padding);
            int[][] points = findPoints(min, max);
            select(points);
        } else {
            throw new IllegalStateException("No selection has been started.");
        }
    }

    @Override
    public void onEndSelect() {
        if (this.originalSelection != null) {
            this.originalSelection = null;
        } else {
            throw new IllegalStateException("No selection has been started.");
        }
    }

    @Override
    public void onCancelSelect() {
        if (this.originalSelection != null) {
            this.selectionProvider.setSelection(this.originalSelection);
            this.originalSelection = null;
        } else {
            throw new IllegalStateException("No selection has been started.");
        }
    }

    @Override
    public void onSelectAll() {
        List<int[]> points = new ArrayList<int[]>();
        int curveCount = this.input.length;
        for (int i = 0; i < curveCount; ++i) {
            HermiteSpline spline = getCurve(i);
            int pointCount = spline.getCount();
            for (int j = 0; j < pointCount; ++j) {
                points.add(new int[] {i, j});
            }
        }
        select(points.toArray(new int[points.size()][]));
    }
}
