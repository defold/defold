(ns editor.html
  (:require
   [dynamo.graph :as g]
   [editor.resource-node :as resource-node]
   [editor.workspace :as workspace]))

(g/defnode HtmlNode
  (inherits resource-node/ResourceNode)

  (output html g/Str (g/fnk [resource] (slurp resource :encoding "UTF-8"))))

(defn register-resource-types
  [workspace]
  (workspace/register-resource-type workspace
                                    :ext "html"
                                    :label "HTML"
                                    :node-type HtmlNode
                                    :view-types [:html]
                                    :view-opts nil))

