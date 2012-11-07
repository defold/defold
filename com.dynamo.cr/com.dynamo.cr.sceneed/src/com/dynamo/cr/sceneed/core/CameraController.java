package com.dynamo.cr.sceneed.core;

import java.util.List;

import javax.annotation.PreDestroy;
import javax.inject.Inject;
import javax.vecmath.AxisAngle4d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.sceneed.core.SceneUtil.MouseType;
import com.dynamo.cr.sceneed.core.util.CameraUtil;
import com.dynamo.cr.sceneed.core.util.CameraUtil.Movement;

public class CameraController implements IRenderViewController {

    private int lastX;
    private int lastY;
    private CameraUtil.Movement movement = Movement.IDLE;
    private IRenderView renderView;
    private Camera camera = new Camera(Camera.Type.ORTHOGRAPHIC);
    private Vector4d focusPoint = new Vector4d(0.0, 0.0, 0.0, 1.0);
    private ISelection selection = new StructuredSelection();
    private ISceneModel sceneModel;

    @Inject
    public CameraController(IRenderView renderView, ISceneModel sceneModel) {
        this.renderView = renderView;
        this.sceneModel = sceneModel;
        this.renderView.addRenderController(this);
        this.renderView.setCamera(camera);
    }

    @PreDestroy
    public void dispose() {
        this.renderView.removeRenderController(this);
    }

    public Vector4d getFocusPoint() {
        return new Vector4d(this.focusPoint);
    }

    public void setFocusPoint(Vector4d focusPoint) {
        this.focusPoint.set(focusPoint);
    }

    @Override
    public void mouseDown(MouseEvent event) {
        lastX = event.x;
        lastY = event.y;
        movement = CameraUtil.getMovement(event);
    }

    private void dolly(double amount) {
        Vector4d focusDelta = new Vector4d(camera.getPosition());
        focusDelta.sub(focusPoint);
        double focusDistance = focusDelta.length();
        if (camera.getType() == Camera.Type.ORTHOGRAPHIC) {
            double fov = camera.getFov();
            fov += amount * fov;
            fov = Math.max(0.01, fov);
            camera.setFov(fov);
        } else {
            Quat4d r = new Quat4d();
            camera.getRotation(r);
            Matrix4d m = new Matrix4d();
            m.setIdentity();
            m.set(r);

            Vector4d delta = new Vector4d();

            m.getColumn(2, delta);
            delta.scale(amount * focusDistance);
            camera.move(delta.x, delta.y, delta.z);
        }
    }

    @Override
    public void mouseMove(MouseEvent e) {
        int dx = lastX - e.x;
        int dy = lastY - e.y;

        if (movement == Movement.ROTATE && camera.getType() == Camera.Type.PERSPECTIVE) {
            Vector4d delta = new Vector4d(camera.getPosition());
            delta.sub(focusPoint);
            Quat4d q_delta = new Quat4d();
            q_delta.set(delta);

            Quat4d r = new Quat4d();
            camera.getRotation(r);
            Quat4d inv_r = new Quat4d(r);
            inv_r.conjugate();

            q_delta.mul(inv_r, q_delta);
            q_delta.mul(r);

            Matrix4d m = new Matrix4d();
            m.setIdentity();
            m.set(r);
            m.transpose();

            Quat4d q2 = new Quat4d();
            q2.set(new AxisAngle4d(1, 0, 0, dy * 0.003));

            Vector4d y_axis = new Vector4d();
            m.getColumn(1, y_axis);
            Quat4d q1 = new Quat4d();
            q1.set(new AxisAngle4d(y_axis.x, y_axis.y, y_axis.z, dx * 0.003));

            q1.mul(q2);
            camera.rotate(q1);

            camera.getRotation(r);
            inv_r.conjugate(r);

            q_delta.mul(r, q_delta);
            q_delta.mul(inv_r);
            delta.set(q_delta);
            delta.add(focusPoint);
            camera.setPosition(delta.x, delta.y, delta.z);
        } else if (movement == Movement.TRACK) {
            Point3d point = camera.project(this.focusPoint.x,
                    this.focusPoint.y, this.focusPoint.z);
            Vector4d world = camera.unProject(e.x, e.y, point.z);
            Vector4d delta = camera.unProject(lastX, lastY, point.z);
            delta.sub(world);
            camera.move(delta.x, delta.y, delta.z);
            this.focusPoint.add(delta);
        } else if (movement == Movement.DOLLY) {
            dolly(-dy * 0.002);
        }

        lastX = e.x;
        lastY = e.y;

        renderView.refresh();
    }

    @Override
    public void mouseUp(MouseEvent e) {
        movement = Movement.IDLE;
    }

    @Override
    public void mouseDoubleClick(MouseEvent e) {
    }

    @Override
    public void mouseScrolled(MouseEvent e) {
        dolly(-e.count * 0.02);
        renderView.refresh();
    }

    private static boolean hasCameraControlModifiers(MouseEvent event) {
        MouseType type = SceneUtil.getMouseType();
        return (type == MouseType.THREE_BUTTON && (event.stateMask & SWT.ALT) != 0)
                || (type == MouseType.ONE_BUTTON && (event.stateMask & (SWT.CTRL|SWT.ALT)) != 0);
    }

    public void frameSelection() {
        AABB aabb = new AABB();

        if (selection.isEmpty()) {
            Node root = sceneModel.getRoot();
            root.getWorldAABB(aabb);
        } else {
            IStructuredSelection structSelection = (IStructuredSelection) selection;
            AABB tmp = new AABB();
            for (Object object : structSelection.toArray()) {
                if (object instanceof Node) {
                    Node node = (Node) object;
                    node.getWorldAABB(tmp);
                    aabb.union(tmp);
                }
            }
        }

        if (aabb.isIdentity())
            return;

        Matrix4d view = new Matrix4d();
        camera.getViewMatrix(view);

        Matrix4d viewInverse = new Matrix4d();
        camera.getViewMatrix(viewInverse);
        viewInverse.invert();

        Point3d center = new Point3d(aabb.min);
        center.add(aabb.max);
        center.scale(0.5);

        setFocusPoint(new Vector4d(center.x, center.y, center.z, 1.0));

        Point3d cameraCenter = new Point3d(center);
        view.transform(cameraCenter);
        cameraCenter.z = 0;

        viewInverse.transform(cameraCenter);
        camera.setPosition(cameraCenter.x, cameraCenter.y, cameraCenter.z);

        if (camera.getType() == Camera.Type.ORTHOGRAPHIC)
        {
            // Adjust orthographic fov to cover everything
            Point3d minProj = camera.project(aabb.min.x, aabb.min.y, aabb.min.z);
            Point3d maxProj = camera.project(aabb.max.x, aabb.max.y, aabb.max.z);

            double fovX = Math.abs(maxProj.x - minProj.x);
            double fovY = Math.abs(maxProj.y - minProj.y);

            int[] viewPort = new int[4];
            camera.getViewport(viewPort);
            double factorX = fovX / (viewPort[2] - viewPort[0]);
            // Convert y-factor to "x-space"
            double factorY = (fovY / (viewPort[3] - viewPort[1])) * camera.getAspect();

            double fovXprim = camera.getFov() * factorX;
            double fovYprim = camera.getFov() * factorY;
            double fovPrim;
            // Is fov-y in x-space larger than fov-x?
            if (fovYprim / camera.getAspect() > fovXprim) {
                // Yes, frame by y
                fovPrim = fovYprim / camera.getAspect();
            } else {
                // Otherwise frame by x
                fovPrim = fovXprim;
            }

            // Show 10% more
            fovPrim *= 1.1;

            camera.setOrthographic(fovPrim, camera.getAspect(), camera.getNearZ(), camera.getFarZ());
        }
        else {
            // TODO: Copy-paste from collection editor and this code
            // is somewhat buggy so don't expect anything! :-)
            // NOTE: perspective is buggy but ortho above is not

            // Adjust position to cover everything

            Point3d p0 = new Point3d(aabb.min.x, aabb.min.y, aabb.min.z);
            Point3d p1 = new Point3d(aabb.min.x, aabb.min.y, aabb.max.z);
            Point3d p2 = new Point3d(aabb.min.x, aabb.max.y, aabb.min.z);
            Point3d p3 = new Point3d(aabb.min.x, aabb.max.y, aabb.max.z);
            Point3d p4 = new Point3d(aabb.max.x, aabb.min.y, aabb.min.z);
            Point3d p5 = new Point3d(aabb.max.x, aabb.min.y, aabb.max.z);
            Point3d p6 = new Point3d(aabb.max.x, aabb.max.y, aabb.min.z);
            Point3d p7 = new Point3d(aabb.max.x, aabb.max.y, aabb.max.z);

            Point3d[] points = new Point3d[] { p0, p1, p2, p3, p4, p5, p6, p7 };

            double minx = Double.MAX_VALUE;
            double maxx = -Double.MAX_VALUE;
            double miny = Double.MAX_VALUE;
            double maxy = -Double.MAX_VALUE;
            double maxz = -Double.MAX_VALUE;

            // Calculate AABB in projection-plane
            for (Point3d p : points) {
                // NOTE: #project applies view-transform
                Point3d projected = camera.project(p.x, p.y, p.z);
                minx = Math.min(minx, projected.x);
                maxx = Math.max(maxx, projected.x);
                miny = Math.min(miny, projected.y);
                maxy = Math.max(maxy, projected.y);
                maxz = Math.max(maxz, projected.z);
            }

            // Unproject into world-space
            Vector4d min = camera.unProject(minx, miny, maxz);
            Vector4d max = camera.unProject(maxx, maxy, maxz);
            // Transform back into viewspace
            view.transform(min);
            view.transform(max);

            // Calculate frustum
            double height = 2 * camera.getNearZ() * Math.tan(camera.getFov() * Math.PI / 360.0);
            double width = height * camera.getAspect();
            // Calculate desired z value to fill the screen for x and y
            double zey = -camera.getNearZ() * (Math.abs(max.y - min.y) / height);
            double zex = -camera.getNearZ() * (Math.abs(max.x - min.x) / width);
            // dz to move
            double dzy = zey - max.z;
            double dzx = zex - max.z;

            double dz = dzy;
            if (dzy > dzx)
                dz = dzx;

            // Move camera dz along third column
            Vector4d dir = new Vector4d();
            viewInverse.getColumn(2, dir);
            dir.scale(dz);
            Vector4d position = camera.getPosition();
            position.sub(dir);
            camera.setPosition(position.x, position.y, position.z);
        }

        this.renderView.refresh();
    }

    @Override
    public IRenderViewController.FocusType getFocusType(List<Node> nodes, MouseEvent event) {
        boolean scroll = event.count != 0 && event.button == 0;
        if (scroll || hasCameraControlModifiers(event)) {
            return FocusType.CAMERA;
        } else {
            return FocusType.NONE;
        }
    }

    @Override
    public void initControl(List<Node> nodes) {
    }

    @Override
    public void finalControl() {
    }

    @Override
    public void keyPressed(KeyEvent e) {
        // TODO Auto-generated method stub

    }

    @Override
    public void keyReleased(KeyEvent e) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setSelection(ISelection selection) {
        this.selection = selection;
    }

    @Override
    public void refresh() {
    }

}
