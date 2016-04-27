(ns editor.gui.nodes
  (:require [clojure.set :as set]
            [dynamo.graph :as g]
            [editor.gui.gui-scene-node :as gsn]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.scene :as scene]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.geom :as geom]
            [editor.gui.util :as gutil]
            [editor.types :as types]
            [editor.gui.icons :as icons])
  (:import [com.dynamo.gui.proto Gui$NodeDesc]))

(defn- v3->v4 [v3 default]
  (conj (or v3 default) 1.0))

(def ^:private layer-connections [[:name :layer-input]
                                  [:index :layer-index]])

(g/defnk produce-node-msg [type parent index _declared-properties _node-id basis]
  (let [pb-renames {:x-anchor :xanchor
                    :y-anchor :yanchor}
        v3-fields {:position [0.0 0.0 0.0] :rotation [0.0 0.0 0.0] :scale [1.0 1.0 1.0] :size [200.0 100.0 0.0]}
        props (:properties _declared-properties)
        indices (set/map-invert (protobuf/fields-by-indices Gui$NodeDesc))
        overrides (map first (filter (fn [[k v]] (contains? v :original-value)) props))
        msg (-> {:parent parent
                 :type type
                 :index index
                 :overridden-fields (mapv indices overrides)
                 :template-node-child (not (nil? (g/override-original basis _node-id)))}
              (into (map (fn [[k v]] [k (:value v)])
                         (filter (fn [[k v]] (and (get v :visible true)
                                                  (not (contains? (set (keys pb-renames)) k))))
                                 props)))
              (cond->
                (= type :type-template) (->
                                          (update :template (fn [t] (resource/proj-path (:resource t))))
                                          (assoc :color [1.0 1.0 1.0 1.0])))
              (into (map (fn [[k v]] [v (get-in props [k :value])]) pb-renames)))
        msg (reduce (fn [msg [k default]] (update msg k v3->v4 default)) msg v3-fields)]
    msg))

(defn node->gui-scene [node]
  (if (g/node-instance? gsn/GuiSceneNode node)
    node
    (let [[_ _ scene _] (first (filter (fn [[_ label _ _]] (= label :_node-id)) (g/outputs node)))]
      scene)))

(g/defnode GuiNode
  (inherits scene/ScalableSceneNode)
  (inherits outline/OutlineNode)

  (property index g/Int (dynamic visible (g/fnk [] false)) (default 0))
  (property type g/Keyword (dynamic visible (g/fnk [] false)))
  (property animation g/Str (dynamic visible (g/fnk [] false)) (default ""))

  (input id-prefix g/Str)
  (property id g/Str (default "")
            (value (g/fnk [id id-prefix] (str id-prefix id)))
            (dynamic read-only? gutil/override?))
  (property size types/Vec3 (dynamic visible (g/fnk [type] (not= type :type-template))) (default [0 0 0]))
  (property color types/Color (dynamic visible (g/fnk [type] (not= type :type-template))) (default [1 1 1 1]))
  (property alpha g/Num (default 1.0)
            (value (g/fnk [color] (get color 3)))
            (set (fn [basis self _ new-value]
                   (if (nil? new-value)
                     (g/clear-property self :color)
                     (g/update-property self :color (fn [v] (assoc v 3 new-value))))))
            (dynamic edit-type (g/fnk [] {:type :slider
                                          :min 0.0
                                          :max 1.0
                                          :precision 0.01})))
  (property inherit-alpha g/Bool (default true))

  (property layer g/Str
            (dynamic edit-type (g/fnk [layer-ids] (properties/->choicebox (cons "" (map first layer-ids)))))
            (value (g/fnk [layer-input] (or layer-input "")))
            (set (fn [basis self _ new-value]
                   (let [layer-ids (g/node-value self :layer-ids :basis basis)]
                     (concat
                       (for [label (map second layer-connections)]
                         (g/disconnect-sources self label))
                       (if (contains? layer-ids new-value)
                         (let [layer-node (layer-ids new-value)]
                           (for [[from to] layer-connections]
                             (g/connect layer-node from self to)))
                         []))))))

  (input parent g/Str)
  (input layer-ids {g/Str g/NodeID})
  (output layer-ids {g/Str g/NodeID} (g/fnk [layer-ids] layer-ids))
  (input layer-input g/Str)
  (input layer-index g/Int)
  (input child-scenes g/Any :array)
  (input child-indices g/Int :array)
  (output node-outline-children [outline/OutlineData] :cached (g/fnk [child-outlines]
                                                                     (vec (sort-by :index child-outlines))))
  (output node-outline-reqs g/Any :cached
          (g/fnk [] [{:node-type GuiNode
                      :tx-attach-fn (fn [target source]
                                      (let [scene (node->gui-scene target)
                                            type (g/node-value source :type)]
                                        (concat
                                          (g/update-property source :id outline/resolve-id (keys (g/node-value scene :node-ids)))
                                          (gutil/attach-gui-node scene target source type))))}]))
  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id id index node-outline-children node-outline-reqs type outline-overridden?]
                 {:node-id _node-id
                  :label id
                  :index index
                  :icon (icons/node-icons type)
                  :child-reqs node-outline-reqs
                  :copy-include-fn (fn [node]
                                     (let [node-id (g/node-id node)]
                                       (and (g/node-instance? GuiNode node-id)
                                            (not= node-id (g/node-value node-id :parent)))))
                  :children node-outline-children
                  :outline-overridden? outline-overridden?}))
  (output pb-msg g/Any :cached produce-node-msg)
  (output pb-msgs g/Any :cached (g/fnk [pb-msg] [pb-msg]))
  (output rt-pb-msgs g/Any (g/fnk [pb-msgs] pb-msgs))
  (output aabb g/Any :abstract)
  (output scene-children g/Any (g/fnk [child-scenes] child-scenes))
  (output scene-renderable g/Any :abstract)
  (output scene g/Any :cached (g/fnk [_node-id aabb transform scene-children scene-renderable]
                                     {:node-id _node-id
                                      :aabb aabb
                                      :transform transform
                                      :children scene-children
                                      :renderable scene-renderable}))

  (input node-ids gutil/IDMap)
  (output node-ids gutil/IDMap (g/fnk [_node-id id node-ids] (into {id _node-id} node-ids)))
  (output node-overrides g/Any :cached (g/fnk [id _properties]
                                             {id (into {} (map (fn [[k v]] [k (:value v)])
                                                               (filter (fn [[_ v]] (contains? v :original-value))
                                                                       (:properties _properties))))})))

(g/defnode NodesNode
  (inherits outline/OutlineNode)

  (property id g/Str (default ""))
  (input child-scenes g/Any :array)
  (input child-indices g/Int :array)
  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id child-outlines]
                 (gutil/gen-outline-fnk-body _node-id child-outlines "Nodes" 0 true
                                  [{:node-type GuiNode
                                    :tx-attach-fn (fn [target source]
                                                    (let [scene (node->gui-scene target)
                                                          type (g/node-value source :type)]
                                                      (concat
                                                       (g/update-property source :id outline/resolve-id
                                                                          (keys (g/node-value scene :node-ids)))
                                                       (gutil/attach-gui-node scene target source type))))}])))
  (output scene g/Any :cached (g/fnk [_node-id child-scenes]
                                     {:node-id _node-id
                                      :aabb (reduce geom/aabb-union (geom/null-aabb) (map :aabb child-scenes))
                                      :children child-scenes})))
