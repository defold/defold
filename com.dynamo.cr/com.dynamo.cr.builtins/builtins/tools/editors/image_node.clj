(ns editors.image-node
  (:require [schema.core :as s]
            [plumbing.core :refer [fnk]]
            [dynamo.node :as n]
            [dynamo.image :as i]
            [dynamo.project :as p]
            [dynamo.system :as ds]
            [dynamo.types :as t]))

(n/defnode ImageResourceNode
  "Produces an image on demand. Can be shown in an outline view."
  (inherits i/ImageSource)
  (inherits n/OutlineNode)
  (output outline-label s/Str (fnk [filename] (t/local-name filename))))

(when (ds/in-transaction?)
  (p/register-node-type "png" ImageResourceNode)
  (p/register-node-type "jpg" ImageResourceNode))
