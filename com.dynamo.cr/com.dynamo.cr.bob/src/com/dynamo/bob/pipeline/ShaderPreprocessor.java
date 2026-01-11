// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.pipeline;

import java.io.*;
import java.nio.file.Files;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.regex.Matcher;
import java.util.Scanner;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.nio.file.Paths;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ShaderUtil.Common;

/* ShaderPreprocessor
 * ========================
 * This class contains functionality to support #include "path-to-file" directives from shader files.
 * Includes must be in double quotes or <> brackets, and the included file
 * must have the .glsl extension since the snippets must be picked up by bob during the build process.
 * During tree creation, the tree will be checked for cycles since we will end up in an endless loop otherwise
 * when building the tree.
 *
 * Relative and absolute paths are supported and must be project relative, examples:
 *
 * #include "/path/to/absolute-file.glsl"
 * #include <file-in-same-dir.glsl>
 * #include "path/to/sub-folder-file.glsl"
 * #include "../file-in-parent-dir.glsl"
 *
 */
public class ShaderPreprocessor {
    // Compiler state
    private Project     project;
    private String      sourcePath;
    private IncludeNode root;
    private String contentRoot;

    private static class IncludeNode {
        public String                             path;
        public String                             source;
        public IncludeNode                        parent;
        public LinkedHashMap<String, IncludeNode> children = new LinkedHashMap<String, IncludeNode>();
    };

    private static class IncludeDirectiveTreeIterator implements Iterator<IncludeNode> {
        private final IncludeNode            root;
        private final ArrayList<IncludeNode> nodeList = new ArrayList<IncludeNode>();
        private int                    nodeIndex;

        public boolean hasNext() {
            return ((nodeIndex + 1 ) < nodeList.size());
        }

        public IncludeNode next() {
            return (nodeList.get(++nodeIndex));
        }

        public void remove() {
            throw new UnsupportedOperationException("Cannot remove a node from include tree");
        }

        private void fillNodeList(IncludeNode node, ArrayList<IncludeNode> nodeList) {
            for (Map.Entry<String, IncludeNode> child : node.children.entrySet()) {
                fillNodeList(child.getValue(), nodeList);
            }
            nodeList.add(node);
        }

        private void reset() {
            this.nodeIndex = -1;
            this.nodeList.clear();
            fillNodeList(this.root, this.nodeList);
        }

        public IncludeDirectiveTreeIterator(IncludeNode node) {
            this.root = node;
            reset();
        }
    }

    public ShaderPreprocessor(Project project, String fromPath, String fromSource, String contentRoot) throws IOException, CompileExceptionError {
        this.project     = project;
        this.sourcePath  = fromPath;
        this.contentRoot = contentRoot;
        this.root        = buildShaderIncludeTree(null, fromPath, Common.stripComments(fromSource));
    }

    public String[] getIncludes() {
        ArrayList<String> res = new ArrayList<String>();
        for (Map.Entry<String, IncludeNode> child : this.root.children.entrySet()) {
            IncludeNode val = child.getValue();
            IncludeDirectiveTreeIterator childIterator = new IncludeDirectiveTreeIterator(child.getValue());
            while(childIterator.hasNext()) {
                res.add(childIterator.next().path);
            }
        }
        return res.toArray(new String[0]);
    }

    // walks all the children of the tree root
    public String getCompiledSource() {
        String source = this.root.source;
        for (Map.Entry<String, IncludeNode> child : this.root.children.entrySet()) {

            ArrayList<String> stringBuffer = new ArrayList<String>();

            IncludeDirectiveTreeIterator iterator = new IncludeDirectiveTreeIterator(child.getValue());
            while(iterator.hasNext()) {
                IncludeNode n = iterator.next();
                stringBuffer.add(n.source);
            }

            String compiledNode                       = "\n" + String.join("\n", stringBuffer);
            String includeReplaceIncludeDirectivesStr = String.format(Common.includeDirectiveReplaceBaseStr, ".*", ".*");
            String nodePatternReplaceDataStr          = String.format(Common.includeDirectiveReplaceBaseStr, child.getKey(), child.getKey());

            compiledNode = compiledNode.replaceAll(includeReplaceIncludeDirectivesStr, "");
            source       = source.replaceAll(nodePatternReplaceDataStr, compiledNode);
        }

        return source;
    }

    private String toProjectRelativePath(String fromFilePath, String includePath) throws CompileExceptionError, IOException {
        // This covers the #include "/absolute-path.glsl" case
        if (includePath.startsWith("/"))
        {
            return includePath.substring(1);
        }

        String rootDir = this.project.getRootDirectory();

        File rootDirFile    = new File(rootDir);
        File fromFileParent = new File(fromFilePath).getParentFile();
        File includeFile    = new File(includePath);

        // Tests have "." as project dir, which gives a null ptr if we don't check it
        if (fromFileParent != null) {
            includeFile = new File(fromFileParent.getCanonicalPath(), includePath);
        }

        Path includeFilePath   = Paths.get(includeFile.getCanonicalPath());
        Path rootDirPath       = Paths.get(rootDirFile.getCanonicalPath());
        String relativePathRes = rootDirPath.relativize(includeFilePath).toString();

        if (relativePathRes.startsWith("..")) {
            throw new CompileExceptionError(fromFilePath + " includes file from outside of project root '" + includePath + "'");
        }

        return relativePathRes;
    }

    private String getIncludeData(String fromPath) throws CompileExceptionError, IOException  {
        String resData = null;
        IResource res = this.project.getResource(fromPath);
        if (res.getContent() != null) {
            resData = new String(res.getContent(), StandardCharsets.UTF_8);
        }

        // Fallback to reading regular data, but only if the content root has been set.
        // This is essentially a workaround for how content is built in the engine.
        if (this.contentRoot != null) {
            fromPath = this.contentRoot + "/" + fromPath;
            File f = new File(fromPath);
            if (f.exists() && f.isFile()) {
                resData = Files.readString(Paths.get(f.getAbsolutePath()));
            }
        }
        if (resData == null) {
            throw new CompileExceptionError(this.sourcePath + " includes '" + fromPath + "', but the file is invalid. " +
                    "Make sure that the path is relative to the project root and that the file is valid!");
        }
        return Common.stripComments(resData);
    }

    private String getPathFromMatcher(Matcher includeMatcher)
    {
        String fromBrackets  = includeMatcher.group("pathbrackets");
        String fromQuotes    = includeMatcher.group("pathquotes");
        return fromBrackets == null ? fromQuotes : fromBrackets;
    }

    private IncludeNode buildShaderIncludeTree(IncludeNode parent, String fromPath, String fromSource) throws IOException, CompileExceptionError {

        IncludeNode newIncludeNode = new IncludeNode();
        newIncludeNode.path        = fromPath;
        newIncludeNode.source      = fromSource;
        newIncludeNode.parent      = parent;

        Scanner scanner = new Scanner(fromSource);
        while (scanner.hasNextLine()) {
            String line = scanner.nextLine();
            Matcher includeMatcher = Common.includeDirectivePattern.matcher(line);
            if(includeMatcher.find()) {
                String path                = getPathFromMatcher(includeMatcher);
                String projectRelativePath = toProjectRelativePath(fromPath, path);

                if (projectRelativePath.equals(fromPath)) {
                    throw new CompileExceptionError(fromPath + " is trying to include itself from " + path);
                }

                // Scan tree backwards to see if the path we want to add already is a parent to this node
                IncludeNode tmp = parent;
                while (tmp != null) {
                    if (tmp.path.equals(projectRelativePath)) {
                        throw new CompileExceptionError(tmp.path + " has a cyclic dependency with " + fromPath);
                    }
                    tmp = tmp.parent;
                }

                newIncludeNode.children.put(path,
                    buildShaderIncludeTree(newIncludeNode, projectRelativePath, getIncludeData(projectRelativePath)));
            }
        }

        return newIncludeNode;
    }
}
