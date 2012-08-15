package com.dynamo.cr.editor.ui;

import org.eclipse.swt.graphics.Image;

public interface IImageProvider {
    /**
     * Returns the icon of the file type associated with the given extension.
     * 
     * The extension can be null since some callers base their icons on their
     * file references. When the file reference is missing, null is supplied to
     * this method.
     * 
     * @param extension
     *            The extension of the file type, all implementations should
     *            support a full file path with extension. null must also be
     *            supported and should result in a generic icon.
     * @return An image that does not need to be disposed.
     */
    Image getIconByExtension(String extension);
}
