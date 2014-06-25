(ns atlas.core
  (:require [dynamo.ui :refer :all]
            [dynamo.node :refer :all]
            [dynamo.file :refer [register-loader protocol-buffer-loader message->node]]
            [dynamo.file.protobuf :refer [protocol-buffer-converters]]
            [dynamo.project :refer [commit-transaction]])
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

(protocol-buffer-converters
 AtlasProto$Atlas
 {:constructor #'atlas.core/make-atlas-node
  :basic-properties [:extrude-borders :margin]
  :node-properties  {:images-list [:tree -> :children]
                     :animations-list [:tree -> :children]}}

 AtlasProto$AtlasAnimation
 {:constructor      #'atlas.core/make-animation-group-node
  :basic-properties [:id :playback :fps :flip-horizontal :flip-vertical]
  :node-properties  {:images-list [:tree -> :children]}}

 AtlasProto$AtlasImage
 {:constructor #'atlas.core/make-image-node
  :basic-properties [:image]})

(defn on-load
  [^String filename ^AtlasProto$Atlas atlas-message]
  (commit-transaction
   (message->node atlas-message nil nil nil)))

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
