package com.dynamo.cr.editor;

import org.eclipse.ui.IFolderLayout;
import org.eclipse.ui.IPageLayout;
import org.eclipse.ui.IPerspectiveFactory;
import org.eclipse.ui.console.IConsoleConstants;

import com.dynamo.cr.editor.compare.ChangedFilesView;

public class Perspective implements IPerspectiveFactory {

	public void createInitialLayout(IPageLayout layout) {
        String editorArea = layout.getEditorArea();
        layout.setEditorAreaVisible(true); // We use standard editors now so true

        layout.addStandaloneView(IPageLayout.ID_PROJECT_EXPLORER,  true, IPageLayout.LEFT, 0.25f, editorArea);
        layout.getViewLayout(IPageLayout.ID_PROJECT_EXPLORER).setCloseable(false);

        IFolderLayout left_folder = layout.createFolder("com.dynamo.cr.editor.left", IPageLayout.BOTTOM, 0.7f, IPageLayout.ID_PROJECT_EXPLORER);

        left_folder.addView(IPageLayout.ID_OUTLINE);
        layout.getViewLayout(IPageLayout.ID_OUTLINE).setCloseable(false);

        left_folder.addView(ChangedFilesView.ID);
        layout.getViewLayout(ChangedFilesView.ID).setCloseable(false);

        IFolderLayout folder = layout.createFolder("com.dynamo.cr.editor.bottom", IPageLayout.BOTTOM, 0.7f, editorArea);
        folder.addView(IConsoleConstants.ID_CONSOLE_VIEW);
        layout.getViewLayout(IConsoleConstants.ID_CONSOLE_VIEW).setCloseable(false);
        folder.addView("org.eclipse.ui.views.ProblemView");
        layout.getViewLayout("org.eclipse.ui.views.ProblemView").setCloseable(false);
	}
}
