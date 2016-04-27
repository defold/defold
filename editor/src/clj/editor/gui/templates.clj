(ns editor.gui.templates
  (:require [editor.math :as math]
            [dynamo.graph :as g]
            [editor.gui.nodes :as nodes]
            [editor.gui.util :as gutil]
            [editor.resource :as resource]
            [editor.defold-project :as project]
            [editor.gui.gui-scene-node :as gsn]
            [editor.outline :as outline]
            [editor.geom :as geom]
            [editor.gl.pass :as pass])
  (:import [javax.vecmath Vector3d Quat4d]))

(def ^:private TemplateData {:resource (g/maybe (g/protocol resource/Resource)) :overrides {g/Str g/Any}})

(defn- trans-position [pos parent-p ^Quat4d parent-q parent-s]
  (let [[x y z] (mapv * pos parent-s)]
    (conj (->>
           (math/rotate parent-q (Vector3d. x y z))
           (math/vecmath->clj)
           (mapv + parent-p))
          1.0)))

(defn- trans-rotation [rot ^Quat4d parent-q]
  (let [q (math/euler->quat rot)]
    (-> q
      (doto (.mul parent-q q))
      (math/quat->euler)
      (conj 1.0))))

(g/defnode TemplateNode
  (inherits nodes/GuiNode)

  (property template TemplateData
            (dynamic read-only? gutil/override?)
            (dynamic edit-type (g/fnk [] {:type resource/Resource
                                          :ext "gui"
                                          :to-type (fn [v] (:resource v))
                                          :from-type (fn [r] {:resource r :overrides {}})}))
            (value (g/fnk [_node-id id template-resource template-overrides]
                          (let [overrides (into {} (map (fn [[k v]] [(subs k (inc (count id))) v]) template-overrides))]
                            {:resource template-resource :overrides overrides})))
            (set (fn [basis self old-value new-value]
                   (let [project (project/get-project self)
                         current-scene (g/node-feeding-into basis self :template-resource)]
                     (concat
                       (if current-scene
                         (g/delete-node current-scene)
                         [])
                       (if (and new-value (:resource new-value))
                         (project/connect-resource-node project (:resource new-value) self []
                                                        (fn [scene-node]
                                                          (let [override (g/override basis scene-node {:traverse? (fn [basis [src src-label tgt tgt-label]]
                                                                                                                    (if (not= src current-scene)
                                                                                                                      (or (g/node-instance? basis nodes/GuiNode src)
                                                                                                                          (g/node-instance? basis nodes/NodesNode src)
                                                                                                                          (g/node-instance? basis gsn/GuiSceneNode src))
                                                                                                                      false))})
                                                                id-mapping (:id-mapping override)
                                                                or-scene (get id-mapping scene-node)
                                                                node-mapping (comp id-mapping (g/node-value scene-node :node-ids :basis basis))]
                                                            (concat
                                                              (:tx-data override)
                                                              (g/connect self :template-prefix or-scene :id-prefix)
                                                              (for [[from to] [[:node-ids :node-ids]
                                                                               [:node-outline :template-outline]
                                                                               [:scene :template-scene]
                                                                               [:build-targets :scene-build-targets]
                                                                               [:resource :template-resource]
                                                                               [:pb-msg :scene-pb-msg]
                                                                               [:rt-pb-msg :scene-rt-pb-msg]
                                                                               [:node-overrides :template-overrides]]]
                                                                (g/connect or-scene from self to))
                                                              (for [[from to] [[:layer-ids :layer-ids]
                                                                               [:texture-ids :texture-ids]
                                                                               [:font-ids :font-ids]]]
                                                                (g/connect self from or-scene to))
                                                              (for [[id data] (:overrides new-value)
                                                                    :let [node-id (node-mapping id)]
                                                                    :when node-id
                                                                    [label value] data]
                                                                (g/set-property node-id label value))))))
                         []))))))

  (display-order (into gutil/base-display-order [:template]))

  (input scene-pb-msg g/Any)
  (input scene-rt-pb-msg g/Any)
  (input scene-build-targets g/Any)
  (output scene-build-targets g/Any (g/fnk [scene-build-targets] scene-build-targets))

  (input template-resource resource/Resource :cascade-delete)
  (input template-outline outline/OutlineData)
  (input template-scene g/Any)
  (input template-overrides g/Any)
  (output template-prefix g/Str (g/fnk [id] (str id "/")))

  (input texture-ids {g/Str g/NodeID})
  (output texture-ids {g/Str g/NodeID} (g/fnk [texture-ids] texture-ids))
  (input font-ids {g/Str g/NodeID})
  (output font-ids {g/Str g/NodeID} (g/fnk [font-ids] font-ids))

  ; Overloaded outputs
  (output node-outline-children [outline/OutlineData] :cached (g/fnk [template-outline]
                                                                     (get-in template-outline [:children 0 :children])))
  (output outline-overridden? g/Bool :cached (g/fnk [template-outline]
                                                    (let [children (get-in template-outline [:children 0 :children])]
                                                      (boolean (some :outline-overridden? children)))))
  (output node-outline-reqs g/Any :cached (g/fnk [] []))
  (output pb-msgs g/Any :cached (g/fnk [id pb-msg scene-pb-msg]
                                       (into [pb-msg] (map #(cond-> % (empty? (:parent %)) (assoc :parent id)) (:nodes scene-pb-msg)))))
  (output rt-pb-msgs g/Any :cached (g/fnk [scene-rt-pb-msg pb-msg]
                                          (let [parent-q (math/euler->quat (:rotation pb-msg))]
                                            (into [] (map #(-> %
                                                             (assoc :index (:index pb-msg))
                                                             (cond->
                                                               (empty? (:layer %)) (assoc :layer (:layer pb-msg))
                                                               (:inherit-alpha %) (->
                                                                                    (update :alpha * (:alpha pb-msg))
                                                                                    (assoc :inherit-alpha (:inherit-alpha pb-msg)))
                                                               (empty? (:parent %)) (->
                                                                                      (assoc :parent (:parent pb-msg))
                                                                                      ;; In fact incorrect, but only possibility to retain rotation/scale separation
                                                                                      (update :scale (partial mapv * (:scale pb-msg)))
                                                                                      (update :position trans-position (:position pb-msg) parent-q (:scale pb-msg))
                                                                                      (update :rotation trans-rotation parent-q))))
                                                         (:nodes scene-rt-pb-msg))))))
  (output node-overrides g/Any :cached (g/fnk [id _properties template-overrides]
                                              (-> {id (into {} (map (fn [[k v]] [k (:value v)])
                                                                    (filter (fn [[_ v]] (contains? v :original-value))
                                                                            (:properties _properties))))}
                                                (merge template-overrides))))
  (output aabb g/Any (g/fnk [template-scene] (:aabb template-scene (geom/null-aabb))))
  (output scene-children g/Any (g/fnk [template-scene] (:children template-scene [])))
  (output scene-renderable g/Any :cached (g/fnk [color inherit-alpha]
                                                {:passes [pass/selection]
                                                 :user-data {:color color :inherit-alpha inherit-alpha}})))
