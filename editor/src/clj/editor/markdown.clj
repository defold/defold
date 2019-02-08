(ns editor.markdown
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace])
  (:import (org.commonmark.node Document)
           (org.commonmark.parser Parser)
           (org.commonmark.renderer.html HtmlRenderer)))

(set! *warn-on-reflection* true)

(defn markdown->html
  [markdown]
  (let [parser (.build (Parser/builder))
        doc (.parse parser markdown)
        renderer (.build (HtmlRenderer/builder))]
    (.render renderer doc)))

(g/defnode OldMarkdownNode
  (inherits resource-node/ResourceNode)

  (property markdown g/Str
            (dynamic visible (g/constantly false)))

  (output html g/Str :cached (g/fnk [_node-id markdown]
                               (str "<!DOCTYPE html>"
                                    "<html><head></head><body>"
                                    (markdown->html markdown)
                                    "</body></html>"))))

(defn load-old-markdown [project self resource]
  (g/set-property self :markdown (slurp resource :encoding "UTF-8")))

;; -----------------------------------------------------------------------------

(g/defnode MarkdownNode
  (inherits resource-node/ResourceNode)
  (property document Document (dynamic visible (g/constantly false))))

(defn- load-markdown [_project self resource]
  (with-open [reader (io/reader resource :encoding "UTF-8")]
    (let [parser (.build (Parser/builder))
          document (.parseReader parser reader)]
      (g/set-property self :document document))))

(defn register-resource-types [workspace]
  (concat
    (workspace/register-resource-type workspace
                                      :ext "md"
                                      :label "Markdown"
                                      :node-type MarkdownNode
                                      :load-fn load-markdown
                                      :view-types [:markdown :text]
                                      :view-opts nil)
    (workspace/register-resource-type workspace
                                      :ext "md2"
                                      :label "Markdown"
                                      :node-type OldMarkdownNode
                                      :load-fn load-old-markdown
                                      :view-types [:html :text]
                                      :view-opts nil)))
