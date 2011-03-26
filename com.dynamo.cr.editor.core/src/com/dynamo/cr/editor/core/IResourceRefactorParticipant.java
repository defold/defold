package com.dynamo.cr.editor.core;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;

public interface IResourceRefactorParticipant {

    /**
     * Update references in resource for moved resource
     * @param context Refactor context
     * @param resource Resource to update references in
     * @param reference Referred reference file
     * @param newPath New path to reference file relative content root
     * @return updated resource in a new buffer. null if not refactoring is applicable.
     * @throws CoreException
     */
     String updateReferences(ResourceRefactorContext context,
                             String resource,
                             IFile reference,
                             String newPath) throws CoreException;

     /**
      * Update references in resource for delete resource
      * @param context Refactor context
      * @param resource Resource to update references in
      * @param reference Referred reference file
      * @return updated resource in a new buffer. null if not refactoring is applicable.
      * @throws CoreException
      */
     String deleteReferences(ResourceRefactorContext context,
                             String resource,
                             IFile reference) throws CoreException;
}
