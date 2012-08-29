package com.dynamo.cr.projed;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.text.ParseException;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.editor.core.ProjectProperties;

public class ProjectEditorPresenter implements ProjectEditorView.IPresenter, IOperationHistoryListener {

    protected static Logger logger = LoggerFactory.getLogger(ProjectEditorPresenter.class);
    private ProjectProperties projectProperties;
    private IProjectEditorView view;
    private UndoContext undoContext;
    private IOperationHistory history;
    private int undoRedoCounter;
    private IProjectEditor projectEditor;

    public ProjectEditorPresenter(IProjectEditor projectEditor, IOperationHistory history) {
        this.projectEditor = projectEditor;
        this.history = history;
        projectProperties = new ProjectProperties();

        this.undoContext = new UndoContext();
        history.setLimit(this.undoContext, 100);
        history.addOperationHistoryListener(this);
    }

    public void dispose() {
        if (history != null) {
            history.removeOperationHistoryListener(this);
        }
    }

    public void load(InputStream is) throws IOException, ParseException {
        projectProperties = new ProjectProperties();
        projectProperties.load(is);
    }

    @Override
    public void setValue(KeyMeta keyMeta, String value) {
        String category = keyMeta.getCategory().getName();
        String key = keyMeta.getName();
        String label = String.format("Set %s.%s", category, key);
        String oldValue = projectProperties.getStringValue(category, key);
        if (value.equals(oldValue)) {
            return;
        }

        if (oldValue == null && value.equals("")) {
            // Preserve, i.e. keep unset, non-set keys updated with ""
            // but update field to default value (default value are always shown
            view.setValue(keyMeta, keyMeta.getDefaultValue());
            return;
        }

        if (value.equals(keyMeta.getDefaultValue())) {
            return;
        }

        if (value.equals("")) {
            value = keyMeta.getDefaultValue();
        }

        IUndoableOperation op = new SetProjectPropertyOperation(label, projectProperties, keyMeta, value, oldValue, this.view);
        op.addContext(undoContext);

        try {
            this.history.execute(op, new NullProgressMonitor(), null);
        } catch (ExecutionException e) {
            logger.error("Failed to execute operation", e);
        }
        validate();
    }

    public void refreshAll() {
        for (CategoryMeta categoryMeta : Meta.getDefaultMeta()) {
            for (KeyMeta keyMeta : categoryMeta.getKeys()) {
                String value = this.projectProperties.getStringValue(categoryMeta.getName(), keyMeta.getName());
                if (value == null) {
                    value = keyMeta.getDefaultValue();
                }
                view.setValue(keyMeta, value);
            }
        }
        validate();
    }

    private void validate() {
        IStatus prevStatus = null;
        this.view.setMessage(null);
        for (CategoryMeta categoryMeta : Meta.getDefaultMeta()) {
            for (KeyMeta keyMeta : categoryMeta.getKeys()) {
                String value = this.projectProperties.getStringValue(categoryMeta.getName(), keyMeta.getName());
                IValidator validator = keyMeta.getValidator();
                if (validator != null) {
                    IStatus status = validator.validate(value);

                    if (prevStatus != null && prevStatus.getSeverity() >= status.getSeverity()) {
                        // Keep status with highest severity
                    } else {
                        if (!status.isOK()) {
                            this.view.setMessage(status);
                            prevStatus = status;
                        }
                    }
                }
            }
        }
    }

    public void setView(IProjectEditorView view) {
        this.view = view;
    }

    public void save(OutputStream os) throws IOException {
        this.projectProperties.save(os);
    }

    public boolean isDirty() {
        return this.undoRedoCounter != 0;
    }

    public void clearDirty() {
        if (this.undoRedoCounter != 0) {
            this.undoRedoCounter = 0;
            this.projectEditor.updateDirty();
        }
    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        if (!event.getOperation().hasContext(this.undoContext)) {
            // Only handle operations related to this editor
            return;
        }
        boolean change = false;
        int type = event.getEventType();
        switch (type) {
        case OperationHistoryEvent.DONE:
        case OperationHistoryEvent.REDONE:
            change = true;
            ++this.undoRedoCounter;
            break;
        case OperationHistoryEvent.UNDONE:
            change = true;
            --this.undoRedoCounter;
            break;
        }
        if (change) {
            this.projectEditor.updateDirty();
            validate();
        }
    }

    public IUndoContext getUndoContext() {
        return undoContext;
    }

}
