package com.dynamo.cr.contenteditor.editors;

import javax.vecmath.AxisAngle4d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.swt.SWT;
import org.eclipse.swt.events.MouseEvent;

public class CameraController
{
    final static int IDLE = 0;
    final static int ROTATE = 1;
    final static int TRACK = 2;
    final static int DOLLY = 3;

    private int m_LastX;
    private int m_LastY;

    int m_State = IDLE;
    boolean m_Mac = false;

    private Vector4d focusPoint = new Vector4d(0.0, 0.0, 0.0, 1.0);

    public CameraController()
    {
        m_Mac = System.getProperty("os.name").toLowerCase().indexOf( "mac" ) >= 0;
    }

    Vector4d getFocusPoint() {
        return this.focusPoint;
    }

    public boolean mouseDown(MouseEvent event)
    {
        m_LastX = event.x;
        m_LastY = event.y;

        if ((m_Mac && event.stateMask == (SWT.ALT | SWT.CTRL))
                || (!m_Mac && event.button == 2 && event.stateMask == SWT.ALT))
        {
            m_State = TRACK;
            return true;
        }
        else if ((m_Mac && event.stateMask == (SWT.ALT))
                || (!m_Mac && event.button == 1 && event.stateMask == SWT.ALT))
        {
            m_State = ROTATE;
            return true;
        }
        else if ((m_Mac && event.stateMask == (SWT.CTRL))
                || (!m_Mac && event.button == 3 && event.stateMask == SWT.ALT))
        {
            m_State = DOLLY;
            return true;
        }
        else
        {
            m_State = IDLE;
            return false;
        }
    }

    public void mouseMove(Camera camera, MouseEvent e)
    {
        int dx = m_LastX - e.x;
        int dy = m_LastY - e.y;

        Vector4d focusDelta = new Vector4d(camera.getPosition());
        focusDelta.sub(focusPoint);
        double focusDistance = focusDelta.length();

        if (m_State == ROTATE && camera.getType() == Camera.Type.PERSPECTIVE)
        {
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
        }
        else if (m_State == TRACK)
        {
            Point3d point = camera.project(this.focusPoint.x, this.focusPoint.y, this.focusPoint.z);
            Vector4d world = camera.unProject(e.x, e.y, point.z);
            Vector4d delta = camera.unProject(m_LastX, m_LastY, point.z);
            delta.sub(world);
            //delta.scale(0.001);
            camera.move(delta.x, delta.y, delta.z);
            this.focusPoint.add(delta);
        }
        else if (m_State == DOLLY)
        {

            if (camera.getType() == Camera.Type.ORTHOGRAPHIC)
            {
                double fov = camera.getFov();
                fov += dy * fov * 0.002;
                fov = Math.max(0.01, fov);
                camera.setFov(fov);
            }
            else
            {
                Quat4d r = new Quat4d();
                camera.getRotation(r);
                Matrix4d m = new Matrix4d();
                m.setIdentity();
                m.set(r);
                //m.transpose();

                Vector4d delta = new Vector4d();

                m.getColumn(2, delta);
                delta.scale(dy * focusDistance * 0.001);
                camera.move(delta.x, delta.y, delta.z);
            }
        }

        m_LastX = e.x;
        m_LastY = e.y;
    }

    public void mouseUp(MouseEvent e)
    {
        m_State = IDLE;
    }
}
