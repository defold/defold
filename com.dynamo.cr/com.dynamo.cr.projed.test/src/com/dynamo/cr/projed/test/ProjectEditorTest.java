package com.dynamo.cr.projed.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.text.ParseException;
import java.util.Collections;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.projed.CategoryMeta;
import com.dynamo.cr.projed.IProjectEditor;
import com.dynamo.cr.projed.IProjectEditorView;
import com.dynamo.cr.projed.KeyMeta;
import com.dynamo.cr.projed.KeyMeta.Type;
import com.dynamo.cr.projed.ProjectEditorPresenter;

public class ProjectEditorTest {

    private IProjectEditorView view;
    private ProjectEditorPresenter presenter;
    private DefaultOperationHistory history;
    private IProjectEditor projectEditor;

    @Before
    public void setUp() throws Exception {
        view = mock(IProjectEditorView.class);
        history = new DefaultOperationHistory();
        projectEditor = mock(IProjectEditor.class);
        presenter = new ProjectEditorPresenter(projectEditor, history);
        presenter.setView(view);
    }

    ProjectProperties loadNew(String name) throws IOException, ParseException {
        ProjectProperties pp = new ProjectProperties();
        InputStream is = getClass().getResourceAsStream("test01.project");
        pp.load(is);
        is.close();
        return pp;
    }

    ProjectProperties loadNew(byte[] bytes) throws IOException, ParseException {
        ProjectProperties pp = new ProjectProperties();
        InputStream is = new ByteArrayInputStream(bytes);
        pp.load(is);
        is.close();
        return pp;
    }

    void load(String name) throws IOException, ParseException {
        InputStream is = getClass().getResourceAsStream(name);
        presenter.load(is);
        is.close();
    }

    byte[] save() throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        presenter.save(out);
        return out.toByteArray();
    }

    private void undo() throws ExecutionException {
        history.undo(presenter.getUndoContext(), new NullProgressMonitor(), null);
    }

    private void redo() throws ExecutionException {
        history.redo(presenter.getUndoContext(), new NullProgressMonitor(), null);
    }

    void setValue(String category, String key, String value) {
        KeyMeta keyMeta = new KeyMeta(new CategoryMeta(category, "", Collections.<KeyMeta> emptyList()), key, Type.STRING, "");
        presenter.setValue(keyMeta, value);
    }

    void setValueWithDefault(String category, String key, String value, String defaultValue) {
        KeyMeta keyMeta = new KeyMeta(new CategoryMeta(category, "", Collections.<KeyMeta> emptyList()), key, Type.STRING, defaultValue);
        presenter.setValue(keyMeta, value);
    }

    @Test
    public void testSet() throws Exception  {
        String project = "test01.project";
        load(project);
        setValue("display", "fullscreen", "1");
        setValue("foo", "bar", "my_value");

        ProjectProperties saved = loadNew(save());
        assertEquals(true, saved.getBooleanValue("display", "fullscreen"));
        assertEquals("my_value", saved.getStringValue("foo", "bar"));
    }

    @Test
    public void testSetDefault() throws Exception  {
        String project = "test01.project";
        load(project);
        setValueWithDefault("display", "width", "", "1280");

        ProjectProperties saved = loadNew(save());
        assertEquals(new Integer(1280), saved.getIntValue("display", "width"));
    }

    @Test
    public void testUndo() throws Exception  {
        String project = "test01.project";
        assertEquals(null, loadNew(project).getStringValue("display", "fullscreen"));
        assertFalse(presenter.isDirty());

        load(project);
        setValue("display", "fullscreen", "1");
        assertTrue(presenter.isDirty());
        undo();
        assertFalse(presenter.isDirty());

        ProjectProperties saved = loadNew(save());
        assertEquals(null, saved.getBooleanValue("display", "fullscreen"));

        redo();
        assertTrue(presenter.isDirty());
        saved = loadNew(save());
        assertEquals(true, saved.getBooleanValue("display", "fullscreen"));
    }

    @Test
    public void testPreserveNonset() throws Exception  {
        String project = "test01.project";
        assertEquals(null, loadNew(project).getStringValue("display", "fullscreen"));

        load(project);
        setValue("display", "fullscreen", "");

        ProjectProperties saved = loadNew(save());
        assertEquals(null, saved.getStringValue("display", "fullscreen"));
    }

}
