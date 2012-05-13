package com.dynamo.cr.protobind.internal;

import java.util.Arrays;

import com.dynamo.cr.protobind.IPath;
import com.dynamo.cr.protobind.PathElement;

/**
 * Representation of a path of field/index elements
 * @author chmu
 *
 */
public class Path implements IPath {
    private PathElement[] elements;

    public Path() {
        elements = new PathElement[0];
    }

    @Override
    public int hashCode() {
        int hash = 7;
        for (PathElement pathElement : this.elements) {
            hash += 31 * pathElement.hashCode();
        }
        return hash;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof IPath) {
            IPath other = (IPath) obj;

            if (this.elements.length == other.elementCount()) {
                for (int i = 0; i < elements.length; ++i) {
                    if (!this.elements[i].equals(other.element(i)))
                        return false;
                }
                return true;
            }
        }
        return false;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.protobind.IPath#element(int)
     */
    @Override
    public PathElement element(int index) {
        return this.elements[index];
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.protobind.IPath#elementCount()
     */
    @Override
    public int elementCount() {
        return elements.length;
    }

    public Path(Path path, PathElement element) {
        elements = Arrays.copyOf(path.elements, path.elements.length + 1);
        elements[path.elements.length] = element;
    }

    @Override
    public String toString() {
        StringBuffer sb = new StringBuffer();
        for (PathElement pe : elements) {
            sb.append("/");
            sb.append(pe.toString());
        }
        return sb.toString();
    }

    @Override
    public String getName() {
        if (elements.length == 0)
            return null;
        else
            return elements[elements.length-1].toString();
    }

    @Override
    public IPath getParent() {
        if (this.elements.length == 0)
            return null;
        Path ret = new Path();
        ret.elements = Arrays.copyOf(this.elements, this.elements.length-1);
        return ret;
    }

    @Override
    public PathElement lastElement() {
        if (this.elements.length == 0)
            return null;
        else
            return this.elements[this.elements.length-1];
    }
}
