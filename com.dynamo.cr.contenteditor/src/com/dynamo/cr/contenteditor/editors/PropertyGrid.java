package com.dynamo.cr.contenteditor.editors;


import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.TimeUnit;

import javax.xml.bind.PropertyException;

import org.eclipse.nebula.widgets.pgroup.PGroup;
import org.eclipse.swt.SWT;
import org.eclipse.swt.custom.ScrolledComposite;
import org.eclipse.swt.events.FocusEvent;
import org.eclipse.swt.events.FocusListener;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Text;
import org.eclipse.swt.widgets.Widget;

import com.dynamo.cr.contenteditor.operations.SetPropertyOperation;
import com.dynamo.cr.contenteditor.scene.IProperty;
import com.dynamo.cr.contenteditor.scene.ISceneListener;
import com.dynamo.cr.contenteditor.scene.Node;
import com.dynamo.cr.contenteditor.scene.Scene;
import com.dynamo.cr.contenteditor.scene.SceneEvent;
import com.dynamo.cr.contenteditor.scene.ScenePropertyChangedEvent;

public class PropertyGrid extends ScrolledComposite implements ISceneListener, Listener, FocusListener
{
    class PropertyThread extends Thread {

        ArrayBlockingQueue<ScenePropertyChangedEvent> queue = new ArrayBlockingQueue<ScenePropertyChangedEvent>(128);
        private boolean quit = false;
        private boolean setPropertiesRequested = false;

        void setPropertiesAsync(final ScenePropertyChangedEvent e) {

            Display.getDefault().asyncExec(new Runnable()
            {
                @Override
                public void run()
                {
                    try
                    {
                        setProperties(e.m_Node, e.m_Property);
                    } catch (PropertyException e)
                    {
                        e.printStackTrace();
                    }
                }
            });
        }

        @Override
        public void run() {
            ArrayList<ScenePropertyChangedEvent> lst = new ArrayList<ScenePropertyChangedEvent>();

            while (!quit) {
                try {
                    ScenePropertyChangedEvent e = queue.poll(5, TimeUnit.SECONDS);
                    if (e != null) {
                        if (setPropertiesRequested) {
                            Thread.sleep(200);
                        }

                        lst.clear();
                        lst.add(e);
                        queue.drainTo(lst);
                        Collections.reverse(lst);
                        while (lst.size() > 0) {
                            e = lst.remove(0);

                            for (int i = 1; i < lst.size(); ++i) {
                                ScenePropertyChangedEvent tmp = lst.get(i);
                                if (tmp.m_Node == e.m_Node && tmp.m_Property == e.m_Property) {
                                    // Remove duplicates
                                    lst.remove(i);
                                    --i;
                                }
                            }
                            setPropertiesRequested = true;
                            setPropertiesAsync(e);
                        }
                    }
                } catch (InterruptedException e) {
                    if (quit)
                        return;
                }
            }
        }

        public void dispose() {
            quit = true;
            this.interrupt();
            try {
                this.join();
            } catch (InterruptedException e) {
            }
        }

        public void addProperty(ScenePropertyChangedEvent event) {
            queue.offer(event);
        }
    }

    private static class PropertyEntry
    {
        public PropertyEntry(IProperty property, Text text)
        {
            m_Property = property;
            m_Text = text;
        }
        IProperty m_Property;
        Text m_Text;
    }

    private final List<PropertyEntry> m_PropertyEntries = new ArrayList<PropertyEntry>();
    private final List<Widget> m_Widgets = new ArrayList<Widget>();
    private Node m_Node;
    private final Scene m_Scene;
    private final IEditor m_Editor;
    private final Composite m_ScrolledArea;
    private PropertyThread m_Thread;

    public PropertyGrid(IEditor editor, Scene scene, Composite parent, int style)
    {
        super(parent, style | SWT.V_SCROLL | SWT.BORDER | SWT.H_SCROLL);

        m_Editor = editor;
        m_Scene = scene;
        scene.addSceneListener(this);

        m_ScrolledArea = new Composite(this, SWT.NONE | SWT.BORDER);
        this.setContent(m_ScrolledArea);
        GridLayout layout = new GridLayout();
        layout.numColumns = 2;
        m_ScrolledArea.setLayout(layout);

        m_Thread = new PropertyThread();
        m_Thread.start();
    }

    @Override
    public void dispose()
    {
        super.dispose();
        m_Scene.removeSceneListener(this);
        m_Thread.dispose();
    }

    Label createLabel(Composite parent, String text)
    {
        Label label = new Label(parent, SWT.HORIZONTAL);
        label.setText(text);
        return label;
    }

    protected void addProperty(Composite parent, IProperty p)
    {
        if (p.getSubProperties() != null && p.getSubProperties().length > 0)
        {
            boolean use_pgroup = false;

            Composite new_parent = null;
            if (!use_pgroup)
            {
                new_parent = parent;
                Label label = createLabel(parent, p.getName() + ":");
                GridData gd = new GridData();
                gd.horizontalSpan = 2;
                label.setLayoutData(gd);
                m_Widgets.add(label);
            }
            else
            {
                PGroup group = new PGroup(parent, SWT.SMOOTH);
                new_parent = group;

                GridData gd = new GridData(GridData.FILL_HORIZONTAL);
                gd.grabExcessHorizontalSpace = true;
                gd.horizontalSpan = 2;
                group.setLayoutData(gd);
                group.setText(p.getName());
                GridLayout layout = new GridLayout();
                layout.numColumns = 2;
                group.setLayout(layout);
                m_Widgets.add(group);
            }

            for (IProperty subp : p.getSubProperties())
            {
                addProperty(new_parent, subp);
            }

        }
        else
        {
            Label label = new Label(parent, SWT.HORIZONTAL);
            label.setText(p.getName() + ":");
            Text text = new Text(parent, SWT.SINGLE | SWT.BORDER);

            text.addListener(SWT.DefaultSelection, this);
            text.addFocusListener(this);


            try
            {
                text.setText(p.getEditableValue(m_Node).toString());
            } catch (PropertyException e)
            {
                e.printStackTrace();
            }

            GridData text_gd = new GridData(GridData.FILL_HORIZONTAL);
            text.setLayoutData(text_gd);

            m_Widgets.add(label);
            m_Widgets.add(text);
            PropertyEntry pe = new PropertyEntry(p, text);
            text.setData(pe);
            m_PropertyEntries.add(pe);
        }
    }

    public void setInput(Node node)
    {
        for (Widget w : m_Widgets)
        {
            w.dispose();
        }

        setMinSize(16, 16); // HACK to avoid growing scrolled area...

        m_Widgets.clear();
        m_PropertyEntries.clear();

        if (node == null)
            return;

        m_Node = node;

        IProperty[] properties = node.getProperties();
        if (properties == null)
            return;

        Label name_label = createLabel(m_ScrolledArea, node.getName());
        GridData name_gd = new GridData();
        name_gd.horizontalSpan = 2;
        name_label.setLayoutData(name_gd);
        m_Widgets.add(name_label);

        for (IProperty p : properties)
        {
            addProperty(m_ScrolledArea, p);
        }

        setContent(m_ScrolledArea);
        Point size = computeSize(SWT.DEFAULT, SWT.DEFAULT, true);
        setMinSize(size);
        setExpandHorizontal(true);
        setExpandVertical(true);

        this.layout(true);
    }

    private void setProperties(Node node, IProperty property) throws PropertyException
    {
        for (PropertyEntry entry : m_PropertyEntries)
        {
            if (entry.m_Property == property)
            {
                //String s = entry.m_Property.getValue(node).toString();
                entry.m_Text.setText(entry.m_Property.getEditableValue(node).toString());
                return;
            }
        }

        IProperty[] sub_props = property.getSubProperties();
        if (sub_props != null && sub_props.length > 0)
        {
            for (IProperty subp : sub_props)
            {
                setProperties(node, subp);
            }
        }
    }

    @Override
    public void propertyChanged(final ScenePropertyChangedEvent event)
    {
        m_Thread.addProperty(event);
    }

    @Override
    public void sceneChanged(SceneEvent event)
    {
    }

    private void handleChangedValue(Text t)
    {
        PropertyEntry pe = (PropertyEntry) t.getData();
        try
        {
            Object original_edit_value = pe.m_Property.getEditableValue(m_Node).toString();
            Object current_edit_value = t.getText();

            if (original_edit_value.equals(current_edit_value))
                return;

            IProperty undo_property = pe.m_Property;
            while (!undo_property.isIndependent())
            {
                undo_property = undo_property.getParentProperty();
            }

            Object undo_original_value = undo_property.getValue(m_Node);

            Object new_value = t.getText();
            String parent_name = "";
            if (pe.m_Property.getParentProperty() != null)
            {
                parent_name = pe.m_Property.getParentProperty().getName() + ".";
            }
            // TODO: Always irreversible? quat.x, normalize(), ...
            // Store parent property for undo-state instead?
            // Kanske for det mesta men hur ar det for euler vs quat och skuggade tillstand?
            SetPropertyOperation op = new SetPropertyOperation("set " + parent_name + pe.m_Property.getName(), m_Node, pe.m_Property, undo_property, undo_original_value, new_value);
            m_Editor.executeOperation(op);
            //pe.m_Property.setValue(m_Node, t.getText());
        } catch (PropertyException e)
        {
            e.printStackTrace();
            try
            {
                t.setText(pe.m_Property.getEditableValue(m_Node).toString());
            } catch (PropertyException e1)
            {
                e1.printStackTrace();
            }
        }
    }

    @Override
    public void handleEvent(Event event)
    {
        Text t = (Text) event.widget;
        handleChangedValue(t);
    }

    @Override
    public void focusGained(FocusEvent e) { }

    @Override
    public void focusLost(FocusEvent event)
    {
        Text t = (Text) event.widget;
        handleChangedValue(t);
    }
}
