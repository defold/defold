(ns editor.markdown
  (:require
   [clojure.java.io :as io]
   [clojure.string :as string]
   [dynamo.graph :as g]
   [editor.handler :as handler]
   [editor.resource :as resource]
   [editor.resource-node :as resource-node]
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
  (inherits resource-node/ResourceNode)

  (property markdown g/Str
            (dynamic visible (g/constantly false)))

  (output html g/Str :cached (g/fnk [_node-id markdown]
                               (str "<!DOCTYPE html>"
                                    "<html><head></head><body>"
                                    (markdown->html markdown)
                                    "</body></html>"))))

(defn load-markdown
  [project self resource]
  (g/set-property self :markdown (slurp resource :encoding "UTF-8")))

(defn register-resource-types
  [workspace]
  (workspace/register-resource-type workspace
                                    :ext "md"
                                    :label "Markdown"
                                    :node-type MarkdownNode
                                    :load-fn load-markdown
                                    :view-types [:html :text]
                                    :view-opts nil))
