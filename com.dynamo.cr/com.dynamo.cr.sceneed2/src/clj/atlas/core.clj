(ns atlas.core
  (:require [dynamo.ui :refer :all]
            [dynamo.node :refer :all]
            [dynamo.file :refer [register-loader protocol-buffer-loader]])
  (:import  [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas]))

(defnode ImageNode
 Persistent
 OutlineChild
 Image)

(defnode AnimationGroupNode
  Persistent
  OutlineParent
  OutlineChild
  Animation)

(def AtlasProperties
  {:inputs {:assets [OutlineItem]}
   :properties {:margin (non-negative-integer)
                :extrude-borders (non-negative-integer)
                :filename (string)}})

(defnode AtlasNode
 OutlineParent
 AtlasProperties)

(defn on-load
  [^String filename ^AtlasProto$Atlas atlas-message]
  (make-atlas-node :margin (.getMargin atlas-message) 
                   :extrude-borders (.getExtrudeBorders atlas-message)
                   :filename filename)
  
  (map #(make-image-node :image (.getImage %)) (.getImagesList atlas-message))
  
  (map #(make-animation-group-node :id (.getId %) 
                                   :playback (.getPlayback %)
                                   :fps (.getFps %)
                                   :flip-horizontal (.getFlipHorizontal %)
                                   :flip-vertical (.getFlipVertical %)
                                   :images (map (fn [i] (make-image-node :image (.getImage i))) (.getImagesList %))) 
       (.getAnimationsList atlas-message))
  )

(register-loader ".atlas" (protocol-buffer-loader AtlasProto$Atlas on-load))

(comment 
  ;; current-selection returns [graph [selected-node-ids]]
  ;; context menu action
  (defn add-image [^ExecutionEvent event]
    (let [resource   (select-resource IMAGE)
          image-node (make-image-node resource)
          target     (current-selection event)]
      (add-to-assets target image-node :image)))

  (defcommand add-image-command "dynamo.atlas" "dynamo.atlas.add-image")
  (defhandler add-image-handler add-image-command #'atlas.core/add-image)
)