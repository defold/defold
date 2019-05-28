
package com.dynamo.bob.bundle;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.file.Files;
import java.nio.charset.StandardCharsets;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.jsoup.Jsoup;
import org.jsoup.nodes.Attribute;
import org.jsoup.nodes.Attributes;
import org.jsoup.nodes.DataNode;
import org.jsoup.nodes.Document;
import org.jsoup.nodes.Element;

public class HtmlMerger {
    private static Logger logger;

    public enum MergePolicy {
        KEEP, MERGE
    }

    HtmlMerger(Logger logger) {
        HtmlMerger.logger = logger;
    }

    class HtmlMergeException extends RuntimeException {
    };

    private static MergePolicy getMergePolicy(Element e) {
        String attr = e.attr("merge");
        if (attr.equals("keep")) return MergePolicy.KEEP;
        return MergePolicy.MERGE;
    }

    public static Element findElement(Element e, String tag, String id) {
        for(Element child : e.children()) {
            if (child.tagName().equals(tag) && child.id().equals(id))
                return child;
        }
        return null;
    }

    public static void mergeNode(Element elementa, Element elementb) {
        for (Attribute attr : elementb.attributes()) {
            elementa.attr(attr.getKey(), attr.getValue());
        }

        for (DataNode node : elementb.dataNodes()) {
            elementa.appendChild(new DataNode(node.getWholeData()));
        }
    }

    public static void mergeElement(Element elementa, Element elementb) {
        MergePolicy mergePolicyA = getMergePolicy(elementa);
        MergePolicy mergePolicyB = getMergePolicy(elementb);

        if (mergePolicyA == MergePolicy.KEEP) {
            // We're keeping the one we've got and discarding the B version
            // We're also not traversing further
            return;
        }
        else if (mergePolicyB == MergePolicy.KEEP) {
            // We're replacing the current one with the B version
            // We're also not traversing further
            elementa.replaceWith(elementb.clone()); // deep clone
            return;
        } else {
            // they both have MERGE
            mergeNode(elementa, elementb);
        }

        for(Element childb : elementb.children()) {
            //String tagb = childb.tagName();
            String idb = childb.hasAttr("id") ? childb.attr("id") : null;

            if (idb == null) { // Simply add to the document
                elementa.appendChild(childb.clone()); // deep clone
            } else {
                // See if there is a similar element in the a's children
                Element childa = findElement(elementa, childb.tagName(), idb);
                if (childa != null) {
                    mergeElement(childa, childb);
                } else {
                    elementa.appendChild(childb.clone()); // deep clone
                }
            }
        }
    }

    public static void mergeDocuments(Document a, Document b) {
        mergeElement(a.head(), b.head());
        mergeElement(a.body(), b.body());
    }

    public static void writeDocument(Document doc, File out) {
        if (out != null) {
            try {
                try (OutputStreamWriter writer = new OutputStreamWriter(new FileOutputStream(out), StandardCharsets.UTF_8)) {
                    String html = doc.outerHtml();
                    writer.write(html);
                }
            } catch (IOException e) {
                throw new RuntimeException("Failed to write html to " + out.getAbsolutePath(), e);
            }
        }
    }

    private static Document loadDocument(File file) throws IOException {
        return Jsoup.parse(file, "UTF-8");
    }

    public static void merge(File main, File[] libraries, File out) throws RuntimeException, IOException {
        Document baseDocument = loadDocument(main);

        // For error reporting/troubleshooting
        String paths = "\n" + main.getAbsolutePath();

        for (File library : libraries) {
            paths += "\n" + library.getAbsolutePath();
            Document libraryDocument = loadDocument(library);
            try {
                mergeDocuments(baseDocument, libraryDocument);
            } catch (HtmlMergeException e) {
                throw new RuntimeException(String.format("Errors merging html files: %s + %s:\n%s", paths, library.getAbsolutePath(), e.toString()));
            }
        }

        HtmlMerger.writeDocument(baseDocument, out);
    }
}
