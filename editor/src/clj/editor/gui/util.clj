(ns editor.gui.util
  (:require [editor.gui.icons :as icons]
            [dynamo.graph :as g]
            [editor.resource :as resource]
            [editor.dialogs :as dialogs]
            [editor.defold-project :as project])
  (:import [com.dynamo.gui.proto Gui$SceneDesc]
           [org.apache.commons.io FilenameUtils]))

(def base-display-order [:id 'scene/ScalableSceneNode :size])

(def IDMap {g/Str g/NodeID})

(def pb-def {:ext "gui"
             :label "Gui"
             :icon icons/gui-icon
             :pb-class Gui$SceneDesc
             :resource-fields [:script :material [:fonts :font] [:textures :texture]]
             :tags #{:component}})

(defn proj-path [resource]
  (if resource
    (resource/proj-path resource)
    ""))

(defn browse [project exts]
  (first (dialogs/make-resource-dialog (project/workspace project) {:ext exts})))

(defn resource->id [resource]
  (FilenameUtils/getBaseName ^String (resource/resource-name resource)))

(defmacro gen-outline-fnk-body [_node-id child-outlines label order sort-children? child-reqs]
  `{:node-id ~_node-id
   :label ~label
   :icon icons/virtual-icon
   :order ~order
   :read-only true
   :child-reqs ~child-reqs
   :children ~(if sort-children?
               `(vec (sort-by :index ~child-outlines))
               child-outlines)})

(g/defnk override? [_node-id basis] (some? (g/override-original basis _node-id)))

(defn attach-gui-node [self parent gui-node type]
  (concat
    (g/connect parent :id gui-node :parent)
    (g/connect gui-node :_node-id self :nodes)
    (g/connect gui-node :node-outline parent :child-outlines)
    (g/connect gui-node :scene parent :child-scenes)
    (g/connect gui-node :index parent :child-indices)
    (g/connect gui-node :pb-msgs self :node-msgs)
    (g/connect gui-node :rt-pb-msgs self :node-rt-msgs)
    (g/connect gui-node :node-ids self :node-ids)
    (g/connect gui-node :node-overrides self :node-overrides)
    (g/connect self :layer-ids gui-node :layer-ids)
    (g/connect self :id-prefix gui-node :id-prefix)
    (case type
      (:type-box :type-pie) (for [[from to] [[:texture-ids :texture-ids]
                                             [:material-shader :material-shader]]]
                              (g/connect self from gui-node to))
      :type-text (g/connect self :font-ids gui-node :font-ids)
      :type-template (concat
                       (for [[from to] [[:texture-ids :texture-ids]
                                        [:font-ids :font-ids]]]
                         (g/connect self from gui-node to))
                       (for [[from to] [[:scene-build-targets :template-build-targets]]]
                         (g/connect gui-node from self to)))
      [])))

(defn pivot->h-align [pivot]
  (case pivot
    (:pivot-e :pivot-ne :pivot-se) :right
    (:pivot-center :pivot-n :pivot-s) :center
    (:pivot-w :pivot-nw :pivot-sw) :left))

(defn pivot->v-align [pivot]
  (case pivot
    (:pivot-ne :pivot-n :pivot-nw) :top
    (:pivot-e :pivot-center :pivot-w) :middle
    (:pivot-se :pivot-s :pivot-sw) :bottom))

(defn pivot-offset [pivot size]
  (let [h-align (pivot->h-align pivot)
        v-align (pivot->v-align pivot)
        xs (case h-align
             :right -1.0
             :center -0.5
             :left 0.0)
        ys (case v-align
             :top -1.0
             :middle -0.5
             :bottom 0.0)]
    (mapv * size [xs ys 1])))
