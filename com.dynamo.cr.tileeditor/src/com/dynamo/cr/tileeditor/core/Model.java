package com.dynamo.cr.tileeditor.core;

import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.MultiStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.tileeditor.Activator;

public class Model {

    private final List<PropertyChangeListener> listeners;
    protected Map<String, IStatus> propertyStatuses = new HashMap<String, IStatus>();

    public Model() {
        this.listeners = new ArrayList<PropertyChangeListener>();
    }

    public void addTaggedPropertyListener(PropertyChangeListener listener) {
        this.listeners.add(listener);
    }

    public void removeTaggedPropertyListener(PropertyChangeListener listener) {
        this.listeners.remove(listener);
    }

    protected void firePropertyChangeEvent(PropertyChangeEvent e) {
        for (PropertyChangeListener listener : this.listeners) {
            listener.propertyChange(e);
        }
    }

    public boolean hasPropertyStatus(String property, int code) {
        IStatus status = this.propertyStatuses.get(property);
        if (status != null) {
            if (status.getCode() != code) {
                if (status.isMultiStatus()) {
                    MultiStatus multiStatus = (MultiStatus)status;
                    for (IStatus s : multiStatus.getChildren()) {
                        if (s.getCode() == code) {
                            return true;
                        }
                    }
                }
                return false;
            } else {
                return true;
            }
        }
        return false;
    }

    public IStatus getPropertyStatus(String property) {
        return this.propertyStatuses.get(property);
    }

    public IStatus getPropertyStatus(String property, int code) {
        IStatus status = this.propertyStatuses.get(property);
        if (status != null) {
            if (status.getCode() != code) {
                if (status.isMultiStatus()) {
                    MultiStatus multiStatus = (MultiStatus)status;
                    for (IStatus s : multiStatus.getChildren()) {
                        if (s.getCode() == code) {
                            return s;
                        }
                    }
                }
            } else {
                return status;
            }
        }
        return null;
    }

    protected void setPropertyStatus(String property, Status status) {
        IStatus oldStatus = this.propertyStatuses.get(property);
        IStatus newStatus = status;
        int code = newStatus.getCode();
        if (oldStatus != null) {
            if (oldStatus.isMultiStatus()) {
                MultiStatus multiStatus = (MultiStatus)oldStatus;
                IStatus[] children = multiStatus.getChildren();
                boolean exists = false;
                for (int i = 0; i < children.length; ++i) {
                    if (children[i].getCode() == code) {
                        children[i] = newStatus;
                        exists = true;
                        break;
                    }
                }
                if (exists) {
                    newStatus = new MultiStatus(Activator.PLUGIN_ID, 0, children, "multistatus", null);
                } else {
                    multiStatus.add(newStatus);
                    newStatus = multiStatus;
                }
            } else {
                if (oldStatus.getCode() != code) {
                    MultiStatus multiStatus = new MultiStatus(Activator.PLUGIN_ID, 0, "multistatus", null);
                    multiStatus.add(newStatus);
                    multiStatus.add(oldStatus);
                    newStatus = multiStatus;
                }
            }
        }
        this.propertyStatuses.put(property, newStatus);
        firePropertyChangeEvent(new PropertyChangeEvent(this, property, oldStatus, newStatus));
    }

    protected void setPropertyStatus(String property, int code, Object binding) {
        setPropertyStatus(property, createStatus(code, binding));
    }

    protected void setPropertyStatus(String property, int code) {
        setPropertyStatus(property, createStatus(code));
    }

    protected Status createStatus(int code, Object[] binding) {
        return new Status(Activator.getStatusSeverity(code), Activator.PLUGIN_ID, code, NLS.bind(Activator.getStatusMessage(code), binding), null);
    }

    private Status createStatus(int code, Object binding) {
        return new Status(Activator.getStatusSeverity(code), Activator.PLUGIN_ID, code, NLS.bind(Activator.getStatusMessage(code), binding), null);
    }

    private Status createStatus(int code) {
        return new Status(Activator.getStatusSeverity(code), Activator.PLUGIN_ID, code, Activator.getStatusMessage(code), null);
    }

    protected void clearPropertyStatus(String property, int code) {
        IStatus status = this.propertyStatuses.get(property);
        IStatus oldStatus = status;
        IStatus newStatus = null;
        if (status != null) {
            boolean exists = false;
            if (status.getCode() == code) {
                exists = true;
            } else {
                if (status.isMultiStatus()) {
                    MultiStatus multiStatus = (MultiStatus)status;
                    IStatus[] children = multiStatus.getChildren();
                    for (int i = 0; i < children.length; ++i) {
                        // erase swap
                        if (children[i].getCode() == code) {
                            exists = true;
                            children[i] = children[children.length - 1];
                            break;
                        }
                    }
                    if (exists) {
                        if (children.length > 2) {
                            IStatus[] newChildren = new IStatus[children.length - 1];
                            System.arraycopy(children, 0, newChildren, 0, children.length - 1);
                            newStatus = new MultiStatus(Activator.PLUGIN_ID, 0, newChildren, "multistatus", null);
                        } else if (children.length == 2) {
                            newStatus = children[0];
                        }
                    }
                }
            }
            if (exists) {
                if (newStatus == null) {
                    this.propertyStatuses.remove(property);
                } else {
                    this.propertyStatuses.put(property, newStatus);
                }
                // fake ok status
                if (newStatus == null) {
                    newStatus = new Status(IStatus.OK, Activator.PLUGIN_ID, "");
                }
                firePropertyChangeEvent(new PropertyChangeEvent(this, property, oldStatus, newStatus));
            }
        }
    }

    public boolean isOk() {
        for (Map.Entry<String, IStatus> propertyStatus : this.propertyStatuses.entrySet()) {
            if (!(propertyStatus.getValue().isOK() || propertyStatus.getValue().getSeverity() == IStatus.INFO)) {
                return false;
            }
        }
        return true;
    }

}
