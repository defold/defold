package com.dynamo.bob.pipeline;

import java.util.HashMap;
import java.util.Map;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.regex.Pattern;
import java.util.regex.Matcher;
import java.io.IOException;
import java.io.FileInputStream;
import java.nio.charset.StandardCharsets;

import org.apache.commons.io.IOUtils;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.pipeline.ShaderUtil.Common;

/* ShaderIncludeCompiler
 * ========================
 * This class contains functionality to support #include ".." directives from shader files.
 * During graph creation, the graph will be checked for cycles since we will end up in an endless loop otherwise
 * when building the graph.
 * 
 * The process currently:
 *   * A shader program builder creates a graph from a shader file by calling buildGraph(source)
 *   * The builder gets a list of all include paths from the graph and marks these as inputs to the shader resource
 *   * When the task is getting built, the graph is passed to the generator together with a list of path <-> data mapping
 *     so the insertIncludeDirective function can replace all occurances of various #include statements with the include
 *     files corresponding data.
 */
public class ShaderIncludeCompiler {
	// Compiler state
	private String projectPath;
    private String shaderSource;
    private String shaderPath;
    private Node   root;

    private class Node {
        public String path;
        public Node parent;
        public HashMap<String, Node> children = new HashMap<String, Node>();
    };

    private class IncludeDirectiveGraphIterator implements Iterator<Node> {
        private Node root;
        private ArrayList<Node> nodeList = new ArrayList<Node>();
        private int nodeIndex;

        public boolean hasNext() {
            return ((nodeIndex + 1 ) < nodeList.size());
        }

        public Node next() {
            return (nodeList.get(++nodeIndex));
        }

        public void remove() {
            throw new UnsupportedOperationException("Can not remove a node");
        }

        private void fillNodeList(Node node, ArrayList<Node> nodeList) {
            for (Map.Entry<String, Node> child : node.children.entrySet()) {
                fillNodeList(child.getValue(), nodeList);
            }
            nodeList.add(node);
        }

        private void reset() {
            this.nodeIndex = -1;
            this.nodeList.clear();
            fillNodeList(this.root, this.nodeList);
        }

        public IncludeDirectiveGraphIterator(Node node) {
            this.root = node;
            reset();
        }
    }

    public ShaderIncludeCompiler(String projectRoot, String shaderPath, String shaderSource) throws IOException, CompileExceptionError {
        this.projectPath  = projectRoot;
        this.shaderSource = shaderSource;
        this.shaderPath   = shaderPath;
        this.root         = buildShaderIncludeGraph(null, shaderSource, shaderPath, new HashMap<String, String>());
    }

    private String toProjectRelativePath(String rootPath, String path) {
        return path;
    }

    public String[] getIncludes() {
        ArrayList<String> res = new ArrayList<String>();
        for (Map.Entry<String, Node> child : this.root.children.entrySet()) {
            IncludeDirectiveGraphIterator childIterator = new IncludeDirectiveGraphIterator(child.getValue());
            while(childIterator.hasNext()) {
                res.add(childIterator.next().path);
            }
        }
        return res.toArray(new String[0]);
    }

    private Node buildShaderIncludeGraph(Node parent, String fromData, String fromPath, HashMap<String, String> nodeCache) throws IOException, CompileExceptionError {
        Node newIncludeNode   = new Node();
        newIncludeNode.path   = fromPath;
        newIncludeNode.parent = parent;

        Matcher includeDirectiveMatcher = Common.includeDirectivePattern.matcher(fromData);
        while (includeDirectiveMatcher.find()) {
            String shaderIncludePath = includeDirectiveMatcher.group("path");

            if (shaderIncludePath != null) {
                shaderIncludePath = toProjectRelativePath(fromPath, shaderIncludePath);

                // Scan graph backwards to see if the path we want to add already is a parent to this node
                Node tmp = parent;
                while (tmp != null)
                {
                    if (tmp.path.equals(shaderIncludePath))
                    {
                        throw new CompileExceptionError(tmp.path + " has a cyclic dependency with " + fromPath);
                    }
                    tmp = parent.parent;
                }

                String childData = null;

                if (nodeCache.containsKey(shaderIncludePath))
                {
                    childData = nodeCache.get(shaderIncludePath);
                } else {
                    try(FileInputStream inputStream = new FileInputStream(shaderIncludePath)) {
                        childData = IOUtils.toString(inputStream);
                    }
                    nodeCache.put(shaderIncludePath, childData);
                }
                assert(childData != null);
                newIncludeNode.children.put(shaderIncludePath,
                    buildShaderIncludeGraph(newIncludeNode, childData, shaderIncludePath, nodeCache));
            }
        }

        return newIncludeNode;
    }

    private Common.DataMapping getDataMappingFromPath(String path, Common.DataMapping[] pathToDataMapping) throws CompileExceptionError {
        for (Common.DataMapping mapping : pathToDataMapping) {
            if (mapping.path.equals(path)) {
                return mapping;
            }
        }
        throw new CompileExceptionError("Could not find include file '%s' when parsing shader file");
    }

    public String insertIncludes(String source, Common.DataMapping[] pathToDataMapping) throws CompileExceptionError {
        if (pathToDataMapping.length == 0) {
            return source;
        }

        for (Map.Entry<String, Node> child : this.root.children.entrySet()) {
            String NodePatternStr = String.format(Common.includeDirectiveBaseStr, child.getKey());
            Pattern NodePattern = Pattern.compile(NodePatternStr);

            ArrayList<String> stringBuffer = new ArrayList<String>();

            IncludeDirectiveGraphIterator iterator = new IncludeDirectiveGraphIterator(child.getValue());
            while(iterator.hasNext()) {
                Node n = iterator.next();
                Common.DataMapping mapping = getDataMappingFromPath(n.path, pathToDataMapping);
                stringBuffer.add(mapping.data);
            }

            String compiledNode = String.join("\n", stringBuffer);
            source = source.replaceAll(NodePatternStr, compiledNode);
        }

        return source;
    }
}