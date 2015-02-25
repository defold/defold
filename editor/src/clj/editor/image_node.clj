(ns editor.image-node
  (:require [schema.core :as s]
            [plumbing.core :refer [fnk]]
            [dynamo.node :as n]
            [dynamo.image :as i]
            [dynamo.types :as t]))

(n/defnode ImageResourceNode
  "Produces an image on demand. Can be shown in an outline view."
  (inherits i/ImageSource)
  (inherits n/OutlineNode)
  (output outline-label s/Str (fnk [filename] (t/local-name filename))))
