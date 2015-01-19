(ns editors.image-node
  (:require [plumbing.core :refer [defnk]]
            [dynamo.node :as n :refer [defnode]]
            [dynamo.image :as i]
            [dynamo.system :as ds]
            [dynamo.project :as p]))

(defnode ImageResourceNode
  (inherits n/OutlineNode)
  (inherits i/ImageSource))

(defn on-load
  [project-node path _]
  (ds/add (n/construct ImageResourceNode :filename path)))

(when (ds/in-transaction?)
  (p/register-node-type "png" ImageResourceNode)
  (p/register-node-type "jpg" ImageResourceNode))