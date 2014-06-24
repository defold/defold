(ns atlas.core
  (:require [dynamo.ui :refer :all]
            [dynamo.node :refer :all]
            [dynamo.file :refer [register-loader protocol-buffer-loader message->node]]
            [dynamo.project :refer [new-resource connect]])
  (:import  [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage]))

(defnode ImageNode
 Persistent
 OutlineNode
 Image)

(defnode AnimationGroupNode
  Persistent
  OutlineNode
  Animation)

(def AtlasProperties
  {:inputs {:assets [OutlineItem]}
   :properties {:margin (non-negative-integer)
                :extrude-borders (non-negative-integer)
                :filename (string)}})

(defnode AtlasNode
 OutlineNode
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
       (.getAnimationsList atlas-message)))

(defmethod message->node AtlasProto$Atlas
  [filename atlas-message]
  (make-atlas-node :margin (.getMargin atlas-message) 
                   :extrude-borders (.getExtrudeBorders atlas-message)
                   :filename filename))

(defmethod message->node AtlasProto$AtlasImage
  [filename image-message]
  (make-image-node :image (.getImage image-message)))

(defmethod message->node AtlasProto$AtlasAnimation
  [filename animation-message]
  (make-animation-group-node :id (.getId animation-message) 
                             :playback (.getPlayback animation-message)
                             :fps (.getFps animation-message)
                             :flip-horizontal (.getFlipHorizontal animation-message)
                             :flip-vertical (.getFlipVertical animation-message)))

(defn on-load
  [^String filename ^AtlasProto$Atlas atlas-message]
  (let [atlas (message->node filename atlas-message)]
       [(new-resource atlas)
         
        (for [img-message (.getImagesList atlas-message)]
          (let [img (message->node filename img-message)]
               [(new-resource img)
                (connect img :tree atlas :children)]))
           
        (for [anim-message (.getAnimationsList atlas-message)]
            (let [anim (message->node filename anim-message)]
                 [(new-resource anim)
                  (connect anim :tree atlas :children)
                  
                  (for [img-message (.getImagesList anim-message)]
                    (let [img (message->node filename img-message)]
                         [(new-resource img)
                          (connect img :tree anim :children)]))]))]))

(comment
  
  (connect [a (make-atlas-node ,,,) :children
            i (map message->node (.getImagesList atlas-message) :parent)
            ])
  (connect [a (make-animation-group-node)
            i (map message->node (.getImagesList animation-group-message) :something)
            i.image -> a.images
            ])
  (connect node-creation-method [parent :linkage
                                 child child-creation-method :child-linkage
                                 ...])
  
  
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