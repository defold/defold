(ns editor.html
  (:require
   [dynamo.graph :as g]
   [editor.defold-project :as project]
   [editor.workspace :as workspace]))

(g/defnode HtmlNode
  (inherits project/ResourceNode)

  (output html g/Str (g/fnk [resource] (slurp resource))))

(defn register-resource-types
  [workspace]
  (workspace/register-resource-type workspace
                                    :ext "html"
                                    :label "HTML"
                                    :node-type HtmlNode
                                    :view-types [:html]
                                    :view-opts nil))

