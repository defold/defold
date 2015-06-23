(ns editor.image-node
  (:require [dynamo.graph :as g]
            [editor.image :as i]
            [dynamo.types :as t]
            [editor.core :as core]))

(g/defnode ImageResourceNode
  "Produces an image on demand. Can be shown in an outline view."
  (inherits i/ImageSource)
  (inherits core/OutlineNode)
  (output outline-label t/Str (g/fnk [filename] (t/local-name filename))))
