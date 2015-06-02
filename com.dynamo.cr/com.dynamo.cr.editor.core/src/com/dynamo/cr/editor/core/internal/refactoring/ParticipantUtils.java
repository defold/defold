package com.dynamo.cr.editor.core.internal.refactoring;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceVisitor;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.ltk.core.refactoring.CompositeChange;
import org.eclipse.ltk.core.refactoring.TextFileChange;
import org.eclipse.text.edits.ReplaceEdit;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.IResourceRefactorParticipant;
import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.editor.core.IResourceTypeRegistry;
import com.dynamo.cr.editor.core.ResourceRefactorContext;

public class ParticipantUtils {

    static class Element {
        public Element(IFile fromFile, IFile toFile) {
            this.fromFile = fromFile;
            this.toFile = toFile;

        }
        IFile fromFile;
        IFile toFile;

        @Override
        public String toString() {
            return String.format("%s -> %s", fromFile, toFile);
        }
    }

    static public class RefactoredFile {
        public RefactoredFile(String originalContent) {
            this.originalContent = originalContent;
            this.newContent = originalContent;
        }
        String originalContent;
        String newContent;
    }

    static void moveFile(Map<IFile, RefactoredFile> refactoredFiles, Element element) throws CoreException, IOException {

        final IResourceTypeRegistry registry = EditorCorePlugin.getDefault();
        String extension = element.fromFile.getFileExtension();
        IResourceType subjectResourceType = registry.getResourceTypeFromExtension(extension);
        if (subjectResourceType == null)
            return;

        final Set<IResourceType> candidateResourceTypes = findCandidateResourceTypes(subjectResourceType);
        IContainer root = EditorUtil.findContentRoot(element.fromFile);
        final List<IFile> candidateFiles = findCandidateFiles(root, candidateResourceTypes);

        ResourceRefactorContext context = new ResourceRefactorContext(root);
        IPath newPath = element.toFile.getFullPath().makeRelativeTo(root.getFullPath());
        String newPathString = "/" + newPath.toPortableString();

        for (IFile candidateFile : candidateFiles) {
            if (!refactoredFiles.containsKey(candidateFile)) {
                InputStream inStream = candidateFile.getContents();
                try {
                    ByteArrayOutputStream outStream = new ByteArrayOutputStream();
                    IOUtils.copy(inStream, outStream);
                    String tmp = new String(outStream.toByteArray(), candidateFile.getCharset());
                    refactoredFiles.put(candidateFile, new RefactoredFile(tmp));
                }
                finally {
                    inStream.close();
                }
            }
            RefactoredFile refactoredFile = refactoredFiles.get(candidateFile);
            String candidateExtension = candidateFile.getFileExtension();
            IResourceType candidateResourceType = registry.getResourceTypeFromExtension(candidateExtension);
            IResourceRefactorParticipant participant = candidateResourceType.getResourceRefactorParticipant();
            String newContent = participant.updateReferences(context, refactoredFile.newContent, element.fromFile, newPathString);
            if (newContent != null) {
                refactoredFile.newContent = newContent;
            }
        }
    }

    public static CompositeChange moveFiles(IProgressMonitor pm, List<Element> elements) throws CoreException {
        HashMap<IFile, RefactoredFile> refactoredFiles = new HashMap<IFile, RefactoredFile>();

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

        return createCompositeChange(refactoredFiles);
    }

    static void deleteFile(Map<IFile, RefactoredFile> refactoredFiles, IFile file, List<IFile> allFiles) throws CoreException, IOException {

        final IResourceTypeRegistry registry = EditorCorePlugin.getDefault();
        String extension = file.getFileExtension();
        IResourceType subjectResourceType = registry.getResourceTypeFromExtension(extension);
        if (subjectResourceType == null)
            return;

        final Set<IResourceType> candidateResourceTypes = findCandidateResourceTypes(subjectResourceType);
        IContainer root = EditorUtil.findContentRoot(file);
        final List<IFile> candidateFiles = findCandidateFiles(root, candidateResourceTypes);

        ResourceRefactorContext context = new ResourceRefactorContext(root);

        for (IFile candidateFile : candidateFiles) {
            // Check that the file is the list of files to delete first
            if (allFiles.contains(candidateFile))
                continue;

            if (!refactoredFiles.containsKey(candidateFile)) {

                InputStream inStream = candidateFile.getContents();
                try {
                    ByteArrayOutputStream outStream = new ByteArrayOutputStream();
                    IOUtils.copy(inStream, outStream);
                    String tmp = new String(outStream.toByteArray(), candidateFile.getCharset());
                    refactoredFiles.put(candidateFile, new RefactoredFile(tmp));
                }
                finally {
                    inStream.close();
                }
            }
            RefactoredFile refactoredFile = refactoredFiles.get(candidateFile);
            String candidateExtension = candidateFile.getFileExtension();
            IResourceType candidateResourceType = registry.getResourceTypeFromExtension(candidateExtension);
            IResourceRefactorParticipant participant = candidateResourceType.getResourceRefactorParticipant();
            String newContent = participant.deleteReferences(context, refactoredFile.newContent, file);
            if (newContent != null) {
                refactoredFile.newContent = newContent;
            }
        }
    }

    public static CompositeChange deleteFiles(IProgressMonitor pm, List<IFile> elements) throws CoreException {
        HashMap<IFile, RefactoredFile> refactoredFiles = new HashMap<IFile, RefactoredFile>();

        pm.beginTask("Refactor resources", elements.size());
        for (IFile element : elements) {
            try {
                deleteFile(refactoredFiles, element, elements);
            } catch (IOException e) {
                throw new CoreException(new Status(IStatus.ERROR, EditorCorePlugin.PLUGIN_ID, e.getMessage()));
            }
            pm.worked(1);
        }
        pm.done();

        return createCompositeChange(refactoredFiles);
    }

    private static CompositeChange createCompositeChange(HashMap<IFile, RefactoredFile> refactoredFiles) {
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
        return null;
    }

    static Set<IResourceType> findCandidateResourceTypes(IResourceType subjectResourceType) {
        Set<IResourceType> visited = new HashSet<IResourceType>();
        return findCandidateResourceTypesInternal(subjectResourceType, visited);
    }

    static Set<IResourceType> findCandidateResourceTypesInternal(IResourceType subjectResourceType, Set<IResourceType> visited) {
        final IResourceTypeRegistry registry = EditorCorePlugin.getDefault();

        visited.add(subjectResourceType);

        /*
         * Loop over all resource types. If the resource type in question can reference subject file
         * the resource type is a candidate for refactoring.
         * Example: subject-file is png-file with type-class image. We find a resource-type, eg Sprite, that can reference images.
         */
        Set<IResourceType> candidateResourceTypes = new HashSet<IResourceType>();
        for (IResourceType resourceType : registry.getResourceTypes()) {
            for (IResourceType referenceResourceType : resourceType.getReferenceResourceTypes()) {
                if (referenceResourceType.equals(subjectResourceType)) {
                    candidateResourceTypes.add(resourceType);
                    break;
                }
            }

            if (subjectResourceType.getTypeClass() != null) {
                for (String referenceTypeClass : resourceType.getReferenceTypeClasses()) {
                    if (referenceTypeClass.equals(subjectResourceType.getTypeClass())) {
                        candidateResourceTypes.add(resourceType);
                        break;
                    }
                }
            }
        }

        Set<IResourceType> newResourceTypes = new HashSet<IResourceType>();
        for (IResourceType rt : candidateResourceTypes) {
            // If embeddable continue searching in hierarchy, but avoid infinitely recursing when there
            // are possible loops.
            if (rt.isEmbeddable() && !visited.contains(rt)) {
                Set<IResourceType> tmp = findCandidateResourceTypesInternal(rt, visited);
                newResourceTypes.addAll(tmp);
            }
        }
        candidateResourceTypes.addAll(newResourceTypes);

        return candidateResourceTypes;
    }

    static List<IFile> findCandidateFiles(IContainer root, final Set<IResourceType> candidateResourceTypes) throws CoreException {
        final IResourceTypeRegistry registry = EditorCorePlugin.getDefault();

        final List<IFile> candidateFiles = new ArrayList<IFile>();
        root.accept(new IResourceVisitor() {
            @Override
            public boolean visit(IResource resource) throws CoreException {
                if (resource instanceof IFile) {
                    IFile file = (IFile) resource;
                    String ext = file.getFileExtension();
                    if (ext != null) {
                        IResourceType rt = registry.getResourceTypeFromExtension(ext);
                        if (rt != null && rt.getResourceRefactorParticipant() != null) {
                            if (candidateResourceTypes.contains(rt)) {
                                candidateFiles.add(file);
                            }
                        }
                    }
                }
                return true;
            }
        });

        return candidateFiles;
    }

}
