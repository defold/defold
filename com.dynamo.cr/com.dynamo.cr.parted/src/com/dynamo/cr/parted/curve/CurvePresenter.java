package com.dynamo.cr.parted.curve;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.vecmath.Point2d;
import javax.vecmath.Vector2d;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.common.EventManager;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.ISelectionProvider;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.ITreeSelection;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.TreePath;
import org.eclipse.jface.viewers.TreeSelection;

import com.dynamo.cr.editor.core.operations.IMergeableOperation;
import com.dynamo.cr.editor.core.operations.IMergeableOperation.Type;
import com.dynamo.cr.parted.curve.ICurveView.IPresenter;
import com.dynamo.cr.parted.operations.InsertPointOperation;
import com.dynamo.cr.parted.operations.MovePointsOperation;
import com.dynamo.cr.parted.operations.RemovePointsOperation;
import com.dynamo.cr.parted.operations.SetTangentOperation;
import com.dynamo.cr.properties.IPropertyDesc;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.PropertyUtil;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.cr.sceneed.core.Node;
import com.google.inject.Inject;

public class CurvePresenter extends EventManager implements IPresenter, ISelectionProvider {

    private static final double MAX_COS_TANGENT_ANGLE = Math.cos(0.2 / 180.0 * Math.PI);

    @Inject
    private ICurveView view;
    @Inject
    private IOperationHistory history;
    @Inject
    private IUndoContext undoContext;

    IPropertyModel<Node, IPropertyObjectWorld> propertyModel;
    IPropertyModel<Node, IPropertyObjectWorld> oldPropertyModel;
    @SuppressWarnings("unchecked")
    private IPropertyDesc<Node, ? extends IPropertyObjectWorld>[] input = new IPropertyDesc[0];
    @SuppressWarnings("unchecked")
    private IPropertyDesc<Node, ? extends IPropertyObjectWorld>[] oldInput = new IPropertyDesc[0];

    private enum DragMode {
        SELECT,
        MOVE_POINTS,
        SET_TANGENT,
    };
    private DragMode dragMode = DragMode.SELECT;
    private List<Point2d> originalPositions = new ArrayList<Point2d>();
    private ISelection originalSelection = new TreeSelection();
    private ISelection selection = new TreeSelection();
    private Point2d dragStart = new Point2d();
    private Vector2d minDragExtents = new Vector2d();
    private Vector2d hitBoxExtents = new Vector2d();
    private boolean dragging = false;
    private int[] tangentIndex = new int[] {-1, -1};

    public void setModel(IPropertyModel<Node, IPropertyObjectWorld> model) {
        this.oldPropertyModel = this.propertyModel;
        this.propertyModel = model;
        updateInput();
    }

    @Override
    public ISelection getSelection() {
        return this.selection;
    }

    @Override
    public void setSelection(ISelection selection) {
        if (!selection.equals(this.selection)) {
            this.selection = selection;
            this.view.setSelection(this.selection);
            fireSelectionChanged(new SelectionChangedEvent(this, selection));
        }
    }

    @SuppressWarnings("unchecked")
    public void updateInput() {
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

        if (this.propertyModel != this.oldPropertyModel || !Arrays.equals(input, oldInput)) {
            this.view.setInput(input);
            setSelection(select(new int[][] {}));
        }

        oldInput = input;
        this.oldPropertyModel = this.propertyModel;
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

    private Map<Integer, List<Integer>> getPointsMapFromSelection() {
        Map<Integer, List<Integer>> points = new HashMap<Integer, List<Integer>>();
        if (!selection.isEmpty() && selection instanceof IStructuredSelection) {
            if (selection instanceof ITreeSelection) {
                ITreeSelection tree = (ITreeSelection)selection;
                for (TreePath path : tree.getPaths()) {
                    int curveIndex = (Integer)path.getSegment(0);
                    List<Integer> list = points.get(curveIndex);
                    if (list == null) {
                        list = new ArrayList<Integer>();
                        points.put(curveIndex, list);
                    }
                    if (path.getSegmentCount() > 1) {
                        int pointIndex = (Integer)path.getSegment(1);
                        points.get(curveIndex).add(pointIndex);
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

    private int[][] getPointsListFromSelection() {
        List<int[]> points = new ArrayList<int[]>();
        if (!selection.isEmpty() && selection instanceof IStructuredSelection) {
            if (selection instanceof ITreeSelection) {
                ITreeSelection tree = (ITreeSelection)selection;
                for (TreePath path : tree.getPaths()) {
                    if (path.getSegmentCount() > 1) {
                        int curveIndex = (Integer)path.getSegment(0);
                        int pointIndex = (Integer)path.getSegment(1);
                        points.add(new int[] {curveIndex, pointIndex});
                    }
                }
            }
        }
        return points.toArray(new int[points.size()][]);
    }

    private IUndoableOperation setCurve(int index, HermiteSpline spline) {
        String id = this.input[index].getId();
        ValueSpread vs = (ValueSpread)this.propertyModel.getPropertyValue(id);
        vs.setCurve(spline);
        return this.propertyModel.setPropertyValue(this.input[index].getId(), vs, true);
    }

    private IUndoableOperation setCurves(Map<Integer, HermiteSpline> curves, boolean force) {
        int curveCount = curves.size();
        Object[] ids = new Object[curveCount];
        Object[] values = new Object[curveCount];
        int index = 0;
        for (Map.Entry<Integer, HermiteSpline> entry : curves.entrySet()) {
            String id = this.input[entry.getKey()].getId();
            ids[index] = id;
            ValueSpread vs = (ValueSpread)this.propertyModel.getPropertyValue(id);
            vs.setCurve(entry.getValue());
            values[index] = vs;
            ++index;
        }
        return PropertyUtil.setProperties(this.propertyModel, ids, values, force);
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

    /**
     * NOTE!
     * This function only works for relatively small boxes.
     * It approximates a part of the curve (intersection with min/max x) by a straight line and test for intersections against that.
     */
    private int findClosestCurve(Point2d min, Point2d max) {
        int closestIndex = -1;
        double closestDistance = Double.MAX_VALUE;
        for (int i = 0; i < this.input.length; ++i) {
            HermiteSpline spline = getCurve(i);
            Point2d p0 = new Point2d(min.getX(), spline.getY(min.getX()));
            Point2d p1 = new Point2d(max.getX(), spline.getY(max.getX()));
            // We now have three cases:
            // * p0.y > max.y: hit iff p1.y < max.y
            // * p0.y < min.y: hit iff p1.y > min.y
            // * otherwise: hit!
            boolean hit = false;
            if (p0.getY() > max.getY()) {
                hit = p1.getY() < max.getY();
            } else if (p0.getY() < min.getY()) {
                hit = p1.getY() > min.getY();
            } else {
                hit = true;
            }
            if (hit) {
                // Project position against p0 -> p1 to find min distance
                Vector2d dir = new Vector2d(p1);
                dir.sub(p0);
                dir.normalize();
                Vector2d closestPosition = new Vector2d(this.dragStart);
                closestPosition.sub(p0);
                double t = closestPosition.dot(dir);
                closestPosition.scaleAdd(t, dir, p0);
                double distance = this.dragStart.distanceSquared(new Point2d(closestPosition));
                if (distance < closestDistance) {
                    closestDistance = distance;
                    closestIndex = i;
                }
            }
        }
        return closestIndex;
    }

    private TreeSelection select(int[][] points) {
        List<TreePath> selection = new ArrayList<TreePath>(points.length);
        for (int i = 0; i < points.length; ++i) {
            Integer[] value = new Integer[points[i].length];
            for (int j = 0; j < value.length; ++j) {
                value[j] = points[i][j];
            }
            selection.add(new TreePath(value));
        }
        return new TreeSelection(selection.toArray(new TreePath[points.length]));
    }

    private boolean hitPosition(Point2d position, Point2d hitPosition, Vector2d hitBoxExtents) {
        Vector2d delta = new Vector2d();
        delta.sub(position, hitPosition);
        return Math.abs(delta.getX()) <= hitBoxExtents.getX()
                && Math.abs(delta.getY()) <= hitBoxExtents.getY();
    }

    private double hitTangent(Point2d position, SplinePoint point, Vector2d hitBoxExtents, Vector2d screenScale, double screenTangentLength) {
        Point2d pointPosition = new Point2d(point.getX(), point.getY());
        Vector2d screenTangent = new Vector2d(point.getTx() * screenScale.getX(), point.getTy() * screenScale.getY());
        screenTangent.normalize();
        screenTangent.scale(screenTangentLength);
        Vector2d tangent = new Vector2d(screenTangent.getX() / screenScale.getX(), screenTangent.getY() / screenScale.getY());
        Point2d t0 = new Point2d(tangent);
        Point2d t1 = new Point2d(tangent);
        t0.scaleAdd(1.0, pointPosition);
        t1.scaleAdd(-1.0, pointPosition);
        if (hitPosition(position, t0, hitBoxExtents)) {
            return position.distance(t0);
        } else if (hitPosition(position, t1, hitBoxExtents)) {
            return position.distance(t1);
        } else {
            return Double.POSITIVE_INFINITY;
        }
    }

    private void startMoveSelection() {
        this.originalPositions.clear();
        int[][] selectedPoints = getPointsListFromSelection();
        for (int[] indices : selectedPoints) {
            HermiteSpline spline = getCurve(indices[0]);
            SplinePoint point = spline.getPoint(indices[1]);
            this.originalPositions.add(new Point2d(point.getX(), point.getY()));
        }
        this.dragMode = DragMode.MOVE_POINTS;
    }

    private int findPoint(HermiteSpline spline, double x) {
        int pointIndex = -1;
        int pointCount = spline.getCount();
        x = Math.max(0, Math.min(1, x));
        for (int i = 0; i < pointCount; ++i) {
            SplinePoint p = spline.getPoint(i);
            if (Math.abs(p.getX() - x) < HermiteSpline.MIN_POINT_X_DISTANCE) {
                return i;
            }
        }
        return pointIndex;
    }

    @Override
    public void onAddPoint(Point2d p) {
        int index = getSingleCurveIndexFromSelection();
        if (index >= 0) {
            HermiteSpline spline = getCurve(index);
            // Find existing points
            int pointIndex = findPoint(spline, p.getX());
            if (pointIndex >= 0) {
                setSelection(select(new int[][] {{index, pointIndex}}));
                this.view.refresh();
            } else {
                spline = spline.insertPoint(p.x, p.y);
                pointIndex = findPoint(spline, p.getX());
                ISelection newSelection = select(new int[][] {{index, pointIndex}});
                execute(new InsertPointOperation(setCurve(index, spline), this.selection, newSelection, this));
            }
        }
    }

    @Override
    public void onRemove() {
        Map<Integer, List<Integer>> selectedPoints = getPointsMapFromSelection();
        Map<Integer, HermiteSpline> curves = new HashMap<Integer, HermiteSpline>();
        if (!selectedPoints.isEmpty()) {
            List<int[]> newSelectedPoints = new ArrayList<int[]>();
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
                curves.put(curveIndex, spline);
            }
            int[][] selection = null;
            if (!newSelectedPoints.isEmpty()) {
                selection = newSelectedPoints.toArray(new int[newSelectedPoints.size()][]);
            } else {
                Integer[] selectedCurves = selectedPoints.keySet().toArray(new Integer[selectedPoints.size()]);
                selection = new int[selectedCurves.length][];
                for (int i = 0; i < selectedCurves.length; ++i) {
                    selection[i] = new int[] {selectedCurves[i]};
                }
            }
            ISelection newSelection = select(selection);
            execute(new RemovePointsOperation(setCurves(curves, true), this.selection, newSelection, this));
        }
    }

    @Override
    public void onStartDrag(Point2d start, Vector2d screenScale, double screenDragPadding, double screenHitPadding, double screenTangentLength) {
        Vector2d invScreenScale = new Vector2d(1.0 / screenScale.getX(), 1.0 / screenScale.getY());
        invScreenScale.absolute();
        this.dragStart.set(start);
        this.dragMode = DragMode.SELECT;
        this.originalSelection = this.selection;
        this.minDragExtents.scale(screenDragPadding, invScreenScale);
        this.hitBoxExtents.scale(screenHitPadding, invScreenScale);
        this.dragging = false;

        Point2d min = new Point2d(hitBoxExtents);
        min.negate();
        min.add(start);
        Point2d max = new Point2d(hitBoxExtents);
        max.add(start);
        int[][] points = findPoints(min, max);
        if (!selection.isEmpty()) {
            Map<Integer, List<Integer>> selectedPoints = getPointsMapFromSelection();
            // Check for normals hit
            if (!selectedPoints.isEmpty()) {
                double minDistance = Double.MAX_VALUE;
                this.tangentIndex = new int[] {-1, -1};
                Set<Integer> selectedCurves = selectedPoints.keySet();
                for (int curveIndex : selectedCurves) {
                    HermiteSpline spline = getCurve(curveIndex);
                    int pointCount = spline.getCount();
                    for (int i = 0; i < pointCount; ++i) {
                        SplinePoint point = spline.getPoint(i);
                        double distance = hitTangent(start, point, hitBoxExtents, screenScale, screenTangentLength);
                        if (distance < minDistance) {
                            minDistance = distance;
                            this.tangentIndex[0] = curveIndex;
                            this.tangentIndex[1] = i;
                        }
                    }
                }
                if (this.tangentIndex[0] >= 0) {
                    this.dragMode = DragMode.SET_TANGENT;
                    return;
                }
            }
            // Check for points to move
            ITreeSelection treeSelection = (ITreeSelection)selection;
            TreePath[] paths = treeSelection.getPaths();
            for (int i = 0; i < paths.length; ++i) {
                TreePath path = paths[i];
                if (path.getSegmentCount() > 1) {
                    int curveIndex = (Integer)path.getSegment(0);
                    HermiteSpline spline = getCurve(curveIndex);
                    int pointIndex = (Integer)path.getSegment(1);
                    SplinePoint point = spline.getPoint(pointIndex);
                    Point2d pointPosition = new Point2d(point.getX(), point.getY());
                    if (hitPosition(start, pointPosition, hitBoxExtents)) {
                        startMoveSelection();
                        return;
                    }
                }
            }
        }
        // Select and move hit points
        if (points.length > 0) {
            int curveIndex = getSingleCurveIndexFromSelection();
            // Prioritize points from selected curve
            boolean foundPoint = false;
            if (curveIndex >= 0) {
                for (int i = 0; i < points.length; ++i) {
                    if (points[i][0] == curveIndex) {
                        setSelection(select(new int[][] {points[i]}));
                        foundPoint = true;
                        break;
                    }
                }
            }
            if (!foundPoint) {
                setSelection(select(points));
            }
            startMoveSelection();
        } else {
            int curveIndex = findClosestCurve(min, max);
            if (curveIndex >= 0) {
                setSelection(select(new int[][] {{curveIndex}}));
            } else {
                setSelection(select(new int[][] {}));
            }
        }
        this.view.refresh();
    }

    @Override
    public void onDrag(Point2d position) {
        Vector2d delta = new Vector2d();
        delta.sub(position, this.dragStart);
        if (this.dragging || (Math.abs(delta.getX()) >= this.minDragExtents.getX() || Math.abs(delta.getY()) >= this.minDragExtents.getY())) {
            boolean initialDrag = !this.dragging;
            this.dragging = true;
            switch (this.dragMode) {
            case MOVE_POINTS:
                int[][] selectedPoints = getPointsListFromSelection();
                int pointCount = selectedPoints.length;
                Point2d p = new Point2d();
                Map<Integer, HermiteSpline> curves = new HashMap<Integer, HermiteSpline>();
                for (int i = 0; i < pointCount; ++i) {
                    int[] indices = selectedPoints[i];
                    int curveIndex = indices[0];
                    int pointIndex = indices[1];
                    HermiteSpline spline = curves.get(curveIndex);
                    if (spline == null) {
                        spline = getCurve(curveIndex);
                    }
                    p.set(this.originalPositions.get(i));
                    p.add(delta);
                    spline = spline.setPosition(pointIndex, p.getX(), p.getY());
                    curves.put(curveIndex, spline);
                }
                IUndoableOperation op = setCurves(curves, true);
                if (initialDrag) {
                    MovePointsOperation moveOp = new MovePointsOperation(op);
                    moveOp.setType(Type.OPEN);
                    execute(moveOp);
                } else {
                    ((IMergeableOperation)op).setType(Type.INTERMEDIATE);
                    execute(op);
                }
                break;
            case SET_TANGENT:
                int curveIndex = this.tangentIndex[0];
                int pointIndex = this.tangentIndex[1];
                HermiteSpline spline = getCurve(curveIndex);
                SplinePoint point = spline.getPoint(pointIndex);
                Vector2d tangent = new Vector2d(point.getX(), point.getY());
                tangent.sub(position);
                if (tangent.lengthSquared() > 0.0) {
                    if (tangent.getX() < 0.0) {
                        tangent.negate();
                    }
                    tangent.normalize();
                    double ty = tangent.getY();
                    if (Math.abs(ty) > MAX_COS_TANGENT_ANGLE) {
                        ty = MAX_COS_TANGENT_ANGLE * Math.signum(ty);
                        tangent.setX(Math.sqrt(1.0 - ty * ty));
                        tangent.setY(ty);
                    }
                    spline = spline.setTangent(pointIndex, tangent.x, tangent.y);
                    IUndoableOperation tangentOp = setCurve(curveIndex, spline);
                    if (initialDrag) {
                        SetTangentOperation setTangentOp = new SetTangentOperation(tangentOp);
                        setTangentOp.setType(Type.OPEN);
                        execute(setTangentOp);
                    } else {
                        ((IMergeableOperation)tangentOp).setType(Type.INTERMEDIATE);
                        execute(tangentOp);
                    }
                }
                break;
            case SELECT:
                Point2d min = new Point2d(Math.min(this.dragStart.x, position.x), Math.min(this.dragStart.y, position.y));
                Point2d max = new Point2d(Math.max(this.dragStart.x, position.x), Math.max(this.dragStart.y, position.y));
                this.view.setSelectionBox(min, max);
                min.sub(this.hitBoxExtents);
                max.add(this.hitBoxExtents);
                int[][] points = findPoints(min, max);
                setSelection(select(points));
                break;
            }
            this.view.refresh();
        }
    }

    @Override
    public void onEndDrag() {
        if (this.dragging) {
            switch (this.dragMode) {
            case MOVE_POINTS:
                int[][] selectedPoints = getPointsListFromSelection();
                int pointCount = selectedPoints.length;
                Map<Integer, HermiteSpline> curves = new HashMap<Integer, HermiteSpline>();
                for (int i = 0; i < pointCount; ++i) {
                    int[] indices = selectedPoints[i];
                    int curveIndex = indices[0];
                    HermiteSpline spline = getCurve(curveIndex);
                    curves.put(curveIndex, spline);
                }
                IUndoableOperation op = setCurves(curves, true);
                execute(op);
                break;
            case SET_TANGENT:
                int curveIndex = this.tangentIndex[0];
                HermiteSpline spline = getCurve(curveIndex);
                IUndoableOperation tangentOp = setCurve(curveIndex, spline);
                ((IMergeableOperation)tangentOp).setType(Type.CLOSE);
                execute(tangentOp);
                break;
            case SELECT:
                this.view.setSelectionBox(new Point2d(), new Point2d());
                break;
            }
            this.originalSelection = null;
        }
        this.view.refresh();
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
        setSelection(select(points.toArray(new int[points.size()][])));
    }

    @Override
    public void onDeselectAll() {
        setSelection(select(new int[][] {}));
    }

    private int[] getClosestPoint(Point2d position, int[][] points) {
        double minDistance = Double.MAX_VALUE;
        int minIndex = -1;
        for (int i = 0; i < points.length; ++i) {
            HermiteSpline spline = getCurve(points[i][0]);
            SplinePoint point = spline.getPoint(points[i][1]);
            Point2d p = new Point2d(point.getX(), point.getY());
            double distance = position.distanceSquared(p);
            if (distance < minDistance) {
                minDistance = distance;
                minIndex = i;
            }
        }
        if (minIndex >= 0) {
            return points[minIndex];
        } else {
            return null;
        }
    }

    @Override
    public void onPickSelect(Point2d start, Vector2d screenScale, double screenHitPadding) {
        Vector2d hitBoxExtents = new Vector2d(1.0 / screenScale.getX(), 1.0 / screenScale.getY());
        hitBoxExtents.absolute();
        hitBoxExtents.scale(screenHitPadding);

        Point2d min = new Point2d(hitBoxExtents);
        min.negate();
        min.add(start);
        Point2d max = new Point2d(hitBoxExtents);
        max.add(start);
        Map<Integer, List<Integer>> selection = getPointsMapFromSelection();
        int[][] points = findPoints(min, max);
        if (points.length > 0) {
            int[] closest = getClosestPoint(start, points);
            List<Integer> selectedPoints = selection.get(closest[0]);
            if (selectedPoints == null || !selectedPoints.contains(closest[1])) {
                setSelection(select(new int[][] {closest}));
            }
        } else {
            int curveIndex = findClosestCurve(min, max);
            if (curveIndex >= 0 && !selection.containsKey(curveIndex)) {
                setSelection(select(new int[][] {{curveIndex}}));
            }
        }
    }

    @Override
    public void addSelectionChangedListener(ISelectionChangedListener listener) {
        addListenerObject(listener);
    }

    @Override
    public void removeSelectionChangedListener(ISelectionChangedListener listener) {
        removeListenerObject(listener);
    }

    private void fireSelectionChanged(SelectionChangedEvent event) {
        Object[] listeners = getListeners();
        for (Object listener : listeners) {
            if (listener instanceof ISelectionChangedListener) {
                ((ISelectionChangedListener)listener).selectionChanged(event);
            }
        }
    }

    // TODO
    // These two methods are used by the add/remove point handlers
    // They could be implemented using core expressions, ISourceProvider and IEvaluationService#requestReeval instead

    public boolean hasRemovablePoints() {
        Map<Integer, List<Integer>> selection = getPointsMapFromSelection();
        for (Map.Entry<Integer, List<Integer>> entry : selection.entrySet()) {
            HermiteSpline spline = getCurve(entry.getKey());
            for (int i : entry.getValue()) {
                if (0 < i && i < spline.getCount() - 1) {
                    return true;
                }
            }
        }
        return false;
    }

    public boolean hasSingleSelectedCurve() {
        ITreeSelection treeSelection = (ITreeSelection)this.selection;
        int curveIndex = -1;
        for (TreePath path : treeSelection.getPaths()) {
            if (path.getSegmentCount() > 0) {
                if (curveIndex < 0) {
                    curveIndex = (Integer)path.getSegment(0);
                } else if (curveIndex != (Integer)path.getSegment(0)) {
                    return false;
                }
            }
        }
        return curveIndex >= 0;
    }
}
