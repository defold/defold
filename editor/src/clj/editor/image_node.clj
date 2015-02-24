(ns editor.image-node
  (:require [dynamo.node :as n]
            [dynamo.image :as i]))

(n/defnode ImageResourceNode
  "Produces an image on demand. Can be shown in an outline view."
  (inherits n/OutlineNode)
  (inherits i/ImageSource))
