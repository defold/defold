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
   (org.commonmark.parser Parser Parser$Builder Parser$ParserExtension)
   (org.commonmark.renderer.html HtmlRenderer)))

(defn markdown->html
  [markdown]
  (let [parser (.build (Parser/builder))
        doc (.parse parser markdown)
        renderer (.build (HtmlRenderer/builder))]
    (.render renderer doc)))

(g/defnode MarkdownNode
  (inherits project/ResourceNode)

  (output html g/Str (g/fnk [_node-id resource]
                       (str "<!DOCTYPE html>"
                            "<html><head></head><body>"
                            (markdown->html (slurp resource :encoding "UTF-8"))
                            "</body></html>"))))

(defn register-resource-types
  [workspace]
  (workspace/register-resource-type workspace
                                    :ext "md"
                                    :label "Markdown"
                                    :node-type MarkdownNode
                                    :view-types [:html]
                                    :view-opts nil))
