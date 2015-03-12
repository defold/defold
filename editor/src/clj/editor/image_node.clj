(ns editor.image-node
  (:require [dynamo.graph :as g]
            [dynamo.image :as i]
            [dynamo.types :as t]))

(g/defnode ImageResourceNode
  "Produces an image on demand. Can be shown in an outline view."
  (inherits i/ImageSource)
  (inherits g/OutlineNode)
  (output outline-label t/Str (g/fnk [filename] (t/local-name filename))))
