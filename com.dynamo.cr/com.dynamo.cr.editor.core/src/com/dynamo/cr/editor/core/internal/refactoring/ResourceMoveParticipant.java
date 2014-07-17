package com.dynamo.cr.editor.core.internal.refactoring;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceVisitor;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.OperationCanceledException;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;
import org.eclipse.ltk.core.refactoring.Change;
import org.eclipse.ltk.core.refactoring.CompositeChange;
import org.eclipse.ltk.core.refactoring.RefactoringStatus;
import org.eclipse.ltk.core.refactoring.TextFileChange;
import org.eclipse.ltk.core.refactoring.participants.CheckConditionsContext;
import org.eclipse.ltk.core.refactoring.participants.ISharableParticipant;
import org.eclipse.ltk.core.refactoring.participants.MoveParticipant;
import org.eclipse.ltk.core.refactoring.participants.RefactoringArguments;
import org.eclipse.text.edits.ReplaceEdit;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.internal.refactoring.ParticipantUtils.Element;
import com.dynamo.cr.editor.core.internal.refactoring.ParticipantUtils.RefactoredFile;

public class ResourceMoveParticipant extends MoveParticipant implements ISharableParticipant {

    private List<Element> elements = new ArrayList<Element>();
    private Map<IFile, RefactoredFile> refactoredFiles;

    public ResourceMoveParticipant() {
    }

    @Override
    protected boolean initialize(Object element) {
        if (!(getArguments().getDestination() instanceof IContainer)) {
            return false;
        }
        IContainer destination = (IContainer)getArguments().getDestination();
        if (destination.getResourceAttributes().isReadOnly()) {
            return false;
        }
        addElement(element, getArguments());

        return true;
    }

    @Override
    public String getName() {
        return "Update references in resources";
    }

    @Override
    public RefactoringStatus checkConditions(IProgressMonitor pm,
            CheckConditionsContext context) throws OperationCanceledException {
        return new RefactoringStatus();
    }

    void refactorFiles(IProgressMonitor pm) throws CoreException {
        refactoredFiles = new HashMap<IFile, RefactoredFile>();

        pm.beginTask("Refactor resources", elements.size());
        for (Element element : elements) {
            try {
                ParticipantUtils.moveFile(refactoredFiles, element);
            } catch (IOException e) {
                throw new CoreException(new Status(IStatus.ERROR, EditorCorePlugin.PLUGIN_ID, e.getMessage()));
            }
            pm.worked(1);
        }
        pm.done();
    }

    @Override
    public Change createChange(IProgressMonitor pm) throws CoreException,
            OperationCanceledException {
        return null;
    }

    @Override
    public Change createPreChange(IProgressMonitor pm) throws CoreException,
            OperationCanceledException {

        if (getArguments().getUpdateReferences()) {
            refactorFiles(pm);
            if (refactoredFiles.size() > 0) {
                CompositeChange change = new CompositeChange("References");
                for (IFile file : refactoredFiles.keySet()) {
                    RefactoredFile refactoredFile = refactoredFiles.get(file);
                    if (!refactoredFile.originalContent.equals(refactoredFile.newContent)) {
                        String originalText = refactoredFile.originalContent;
                        String newText = refactoredFile.newContent;

                        TextFileChange textFileChange = new TextFileChange(file.getName(), file);
                        ReplaceEdit edit = new ReplaceEdit(0, originalText.length(), newText);
                        textFileChange.setEdit(edit);
                        change.add(textFileChange);
                    }
                }
                return change;
            }
        }
        return null;
    }

    @Override
    public void addElement(Object element, RefactoringArguments arguments) {
        final IContainer destination = (IContainer) getArguments().getDestination();
        if (element instanceof IFile) {
            IFile file = (IFile) element;
            // Only add files with extension
            if (file.getFileExtension() != null) {
                IFile toFile = destination.getFile(new Path(file.getName()));
                elements.add(new Element(file, toFile));
            }
        }
        else if (element instanceof IFolder) {
            final IFolder folder = (IFolder) element;
            try {
                folder.accept(new IResourceVisitor() {

                    @Override
                    public boolean visit(IResource resource) throws CoreException {
                        if (resource instanceof IFile) {
                            IFile file = (IFile) resource;
                            // Only add files with extension
                            if (file.getFileExtension() != null) {
                                IPath pathTo = file.getFullPath().makeRelativeTo(folder.getParent().getFullPath());
                                IFile toFile = destination.getFile(pathTo);
                                elements.add(new Element(file, toFile));
                            }
                        }
                        return true;
                    }
                });
            } catch (CoreException e) {
                // Can't use any UI here in a core plugin such as StatusManager
                e.printStackTrace();
            }
        }
    }
}
