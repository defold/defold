(ns atlas.core
  (:require [dynamo.ui :refer :all]
            [dynamo.node :refer :all]))

(defnode ImageNode
  Persistent
  OutlineChild
  Image)

(defnode AnimationGroupNode
  Persistent
  OutlineParent
  OutlineChild
  Animation)

(defnode AtlasNode 
  Rotatable 
  Translatable 
  OutlineParent
  {:inputs {:assets [OutlineItem]}})


;;; current-selection returns [graph [selected-node-ids]]

(comment 
  ;; context menu action
  (defn add-image [^ExecutionEvent event]
    (let [resource   (select-resource IMAGE)
          image-node (make-image-node resource)
          target     (current-selection event)]
      (add-to-assets target image-node :image)))

  (defcommand add-image-command "dynamo.atlas" "dynamo.atlas.add-image")
  (defhandler add-image-handler add-image-command #'atlas.core/add-image)
)