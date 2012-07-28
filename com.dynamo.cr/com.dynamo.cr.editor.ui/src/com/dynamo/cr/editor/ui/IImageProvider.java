package com.dynamo.cr.editor.ui;

import org.eclipse.swt.graphics.Image;

public interface IImageProvider {
    /**
     * Returns the icon of the file type associated with the given extension.
     *
     * @param extension
     *            The extension of the file type, all implementations should
     *            support a full file path with extension.
     * @return An image that does not need to be disposed.
     */
    Image getIconByExtension(String extension);
}
