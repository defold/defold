(ns atlas.core
  (:require [dynamo.ui :refer :all]
            [dynamo.node :refer :all]))

(defnode ImageNode
  Persistent
  OutlineParent
  OutlineChild
  Playable
  Image)

(defnode AnimationGroup
  Persistent
  OutlineParent
  OutlineChild
  Playable
  Animation)

:rotation (rotation)
:position (translation)
:scale    (isotropic-scale)
:scale    (anisotropic-scale)
              


(defnode AtlasNode
  Persistent
  :resource (output (resource))
  :loader   (function)
  :saver    (function)
  
  CompilationTarget
  :resource (output (resource))
  :format   (output (binary-of ,,,))
 
  ---
  :resources (list-of Playable)
  :rotation (rotation)
  :position (translation)
  :scale    (isotropic-scale)
  :scale    (anisotropic-scale)
  
  PropertySource
  :margin (non-negative-integer)
  :extrude-borders (non-negative-integer)
  
  OutlineParent
  OutlineChild
  
  Actionable
  :delete-child :context OutlineParent
  
  )

(defnode CollisionSphere
  Rotatable
  Translatable
  IsotropicallyScalable
  OutlineChild
  Renderable
  {:inputs {:something-special s/Int}})


;;; defnode must:
;;;  - create a type that implements the given protocols
;;;  - register that type (for what?)
;;;  - create a constructor function
;;;  - create getter / setter functions ?? 

;;; current-selection returns [graph [selected-node-ids]]

;; context menu action
(defn add-image [^ExecutionEvent event]
  (let [resource   (select-resource IMAGE)
        image-node (make-image-node resource)
        target     (current-selection event)]
    (add-to-assets target image-node :image)))



(defcommand add-image-command "dynamo.atlas" "dynamo.atlas.add-image")
(defhandler add-image-handler add-image-command #'atlas.core/add-image)