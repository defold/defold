package com.dynamo.cr.editor;

import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IProjectNature;
import org.eclipse.core.runtime.CoreException;

public class ProjectNature implements IProjectNature {

    private IProject project;

    @Override
    public void configure() throws CoreException {
        System.out.println("CONFIGURE NATURE");
        // TODO Auto-generated method stub
    }

    @Override
    public void deconfigure() throws CoreException {
        System.out.println("DECONFIGURE NATURE");
        // TODO Auto-generated method stub
    }

    @Override
    public IProject getProject() {
        return this.project;
    }

    @Override
    public void setProject(IProject project) {
        this.project = project;
    }
}
