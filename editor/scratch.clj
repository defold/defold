(ns scratch
  (:require [clojure.pprint :refer :all]
            [clojure.repl :refer :all]
            [clojure.set :as set]
            [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.integration.override-test :as ot]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.gui :as gui]
            [editor.outline :as outline]
            [editor.particlefx :as particlefx]
            [editor.resource :as resource]
            [editor.scene :as scene]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.property :as ip]
            [internal.util :as util]
            [support.test-support :refer :all]))

(comment

  (defmacro exp [x & ys]
    `(-> (in/node-type-forms '~x (concat g/node-intrinsics '~ys))
         (in/make-node-type-map)))

  (exp ResourceNode
    (property filename types/PathManipulation (dynamic visible (g/fnk [] false)))

    (output content g/Any :abstract))

  (g/defnode ResourceNode)

  (exp GuiNode
       (inherits scene/ScalableSceneNode)
       (inherits outline/OutlineNode)

       (property index g/Int (dynamic visible false) (default 0))
       (property type g/Keyword (dynamic visible false))
       (property animation g/Str (dynamic visible false) (default ""))

       (input id-prefix g/Str)
       (property id g/Str (default "")
                 (value (g/fnk [id id-prefix] (str id-prefix id)))
                 (dynamic read-only? override?))
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
                                           (let [scene (gui/node->gui-scene target)
                                                 type (g/node-value source :type)]
                                             (concat
                                              (g/update-property source :id outline/resolve-id (keys (g/node-value scene :node-ids)))
                                              (gui/attach-gui-node scene target source type))))}]))
       (output node-outline outline/OutlineData :cached
               (g/fnk [_node-id id index node-outline-children node-outline-reqs type outline-overridden?]
                      {:node-id _node-id
                       :label id
                       :index index
                       :icon (node-icons type)
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

       (input node-ids gui/IDMap)
       (output node-ids gui/IDMap (g/fnk [_node-id id node-ids] (into {id _node-id} node-ids)))
       (output node-overrides g/Any :cached (g/fnk [id _properties]
                                                   {id (into {} (map (fn [[k v]] [k (:value v)])
                                                                     (filter (fn [[_ v]] (contains? v :original-value))
                                                                             (:properties _properties))))})))






  (pst 50))
