(ns editor.image-node
  (:require [dynamo.graph :as g]
            [editor.image :as i]
            [editor.types :as types]
            [editor.core :as core]))

(g/defnode ImageResourceNode
  "Produces an image on demand. Can be shown in an outline view."
  (inherits i/ImageSource)
  (inherits core/OutlineNode)
  (output outline-label g/Str (g/fnk [filename] (types/local-name filename))))
