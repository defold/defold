(ns editor.markdown
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace])
  (:import (org.commonmark.node Document Node)
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
  (let [parser (.build (Parser/builder))
        raw-md (slurp resource :encoding "UTF-8")
        filtered-md (string/replace raw-md #"\</?kbd\>" "`")
        document (.parse parser filtered-md)]
    (g/set-property self :document document)))

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

;; -----------------------------------------------------------------------------

(defn- md-node-children [^Node md-node]
  (take-while some?
              (iterate #(some-> ^Node % .getNext)
                       (.getFirstChild md-node))))

(defn markdown-tree [^Node md-node]
  (let [children (mapv markdown-tree (md-node-children md-node))]
    (cond-> (dissoc (bean md-node)
                    :firstChild :lastChild :next :parent :previous)
            (seq children) (assoc :children children))))
