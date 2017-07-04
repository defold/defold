(ns editor.markdown
  (:require
   [clojure.java.io :as io]
   [clojure.string :as string]
   [dynamo.graph :as g]
   [editor.defold-project :as project]
   [editor.handler :as handler]
   [editor.resource :as resource]
   [editor.ui :as ui]
   [editor.view :as view]
   [editor.workspace :as workspace])
  (:import
   (org.commonmark Extension)
   (org.commonmark.node CustomBlock FencedCodeBlock Link Visitor)
   (org.commonmark.parser Parser Parser$Builder Parser$ParserExtension)
   (org.commonmark.renderer.html HtmlRenderer)))

(defn- process-defold-code-block
  [^FencedCodeBlock node project]
  (let [info (.getInfo node)]
    (when (.startsWith info "defold")
      (let [proj-path (string/trim (.getLiteral node))
            resource (workspace/find-resource (project/workspace project) proj-path)]
        (.setLiteral node (slurp resource))))))

(defn- make-visitor
  [project]
  (letfn [(visit-children [^Visitor visitor ^org.commonmark.node.Node parent]
            (loop [node (.getFirstChild parent)]
              (when node
                (let [next (.getNext node)]
                  (.accept node visitor)
                  (recur next)))))]
    (reify Visitor
      (^void visit [this ^org.commonmark.node.BlockQuote node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.BulletList node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.Code node] (visit-children node))
      (^void visit [this ^org.commonmark.node.CustomBlock node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.CustomNode node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.Document node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.Emphasis node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.FencedCodeBlock node]
       (process-defold-code-block node project)
       (visit-children this node))
      (^void visit [this ^org.commonmark.node.HardLineBreak node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.Heading node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.HtmlBlock node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.HtmlInline node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.Image node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.IndentedCodeBlock node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.Link node] (visit-children this node))      
      (^void visit [this ^org.commonmark.node.ListItem node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.OrderedList node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.Paragraph node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.SoftLineBreak node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.StrongEmphasis node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.Text node] (visit-children this node))
      (^void visit [this ^org.commonmark.node.ThematicBreak node] (visit-children this node)))))

(defn markdown->html
  ([markdown]
   (markdown->html markdown nil))
  ([markdown visitor]
   (let [parser (.build (Parser/builder))
         doc (.parse parser markdown)
         renderer (.build (HtmlRenderer/builder))]
     (when visitor (.accept doc visitor))
     (.render renderer doc))))

(g/defnode MarkdownNode
  (inherits project/ResourceNode)

  (output html g/Str (g/fnk [_node-id resource]
                       (str "<!DOCTYPE html>"
                            "<html><head></head><body>"
                            (markdown->html (slurp resource :encoding "UTF-8")
                                            #_(make-visitor (project/get-project _node-id)))
                            "</body></html>"))))

(defn register-resource-types
  [workspace]
  (workspace/register-resource-type workspace
                                    :ext "md"
                                    :label "Markdown"
                                    :node-type MarkdownNode
                                    :view-types [:html]
                                    :view-opts nil))
