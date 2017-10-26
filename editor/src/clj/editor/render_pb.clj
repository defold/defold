(ns editor.render-pb
  (:require
   [dynamo.graph :as g]
   [editor.core :as core]
   [editor.graph-util :as gu]
   [editor.resource :as resource]
   [editor.resource-node :as resource-node]
   [editor.validation :as validation]
   [editor.workspace :as workspace]
   [editor.protobuf :as protobuf]
   [editor.defold-project :as project])
  (:import
   [com.dynamo.render.proto Render$RenderPrototypeDesc]))

(g/defnode NamedMaterial
  (property name g/Str
            (dynamic visible (g/constantly false)))
  (property material resource/Resource
            (value (gu/passthrough material-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (let [project (project/get-project self)
                         connections [[:resource :material-resource]
                                      [:build-targets :dep-build-targets]]]
                     (concat
                       (if old-value
                         (project/disconnect-resource-node project old-value self connections)
                         (g/disconnect project :nil-resource self :material-resource))
                       (if new-value
                         (project/connect-resource-node project new-value self connections)
                         (g/connect project :nil-resource self :material-resource))))))
           (dynamic visible (g/constantly false)))

  (input material-resource resource/Resource)
  (input dep-build-targets g/Any)

  (output named-material g/Any (g/fnk [_node-id name material]
                                 {:name name
                                  :material material}))
  (output dep-build-targets g/Any (gu/passthrough dep-build-targets)))

(defn- make-named-material-node
  [graph-id render-node name material-resource]
  (g/make-nodes
    graph-id
    [named-material [NamedMaterial :name name :material material-resource]]
    (g/connect named-material :_node-id render-node :nodes)
    (g/connect named-material :named-material render-node :named-materials)
    (g/connect named-material :dep-build-targets render-node :dep-build-targets)))


(def ^:private form-sections
  {
   :sections
   [
    {
     :title "Render"
     :fields
     [
      {
       :path [:script]
       :type :resource
       :filter "render_script"
       :label "Script"
       }
      {
       :path [:named-materials]
       :type :table
       :label "Materials"
       :columns [{:path [:name] :label "Name" :type :string :default "New Material"}
                 {:path [:material] :label "Material" :type :resource :filter "material" :default nil}]
       }
      ]
     }
    ]
   })

(defn- set-form-op [{:keys [node-id] :as user-data} path value]
  (condp = path
    [:script]          (g/set-property! node-id :script value)
    [:named-materials] (let [graph-id (g/node-id->graph-id node-id)]
                         (g/transact
                           (concat
                             (for [[named-material-id _] (g/sources-of node-id :named-materials)]
                               (g/delete-node named-material-id))
                             (for [{:keys [name material]} value]
                               (make-named-material-node graph-id node-id name material)))))))

(g/defnk produce-form-data [_node-id script-resource named-materials]
  (-> form-sections
      (assoc :form-ops {:user-data {:node-id _node-id}
                        :set set-form-op})
      (assoc :values {[:script] script-resource
                      [:named-materials] named-materials})))

(g/defnk produce-pb-msg [script-resource named-materials]
  {:script (resource/resource->proj-path script-resource)
   :materials (mapv (fn [{:keys [name material]}]
                      {:name name
                       :material (resource/resource->proj-path material)})
                    named-materials)})

(defn- build-render [resource dep-resources user-data]
  (let [{:keys [pb-msg built-resources]} user-data
        built-pb (reduce (fn [pb [path built-resource]]
                           (assoc-in pb path (resource/proj-path (get dep-resources built-resource))))
                         pb-msg
                         built-resources)]
    {:resource resource :content (protobuf/map->bytes Render$RenderPrototypeDesc built-pb)}))

(defn- build-errors
  [_node-id script named-materials]
  (when-let [errors (->> (into [(validation/prop-error :fatal _node-id :script validation/prop-resource-missing? script "Script")]
                               (for [{:keys [name material]} named-materials]
                                 (validation/prop-error :fatal _node-id :material validation/prop-resource-missing? material name)))
                         (remove nil?)
                         (seq))]
    (g/error-aggregate errors)))

(g/defnk produce-build-targets [_node-id resource pb-msg dep-build-targets script named-materials]
  (or (build-errors _node-id script named-materials) 
      (let [dep-build-targets (flatten dep-build-targets)
            deps-by-source (into {} (map #(let [build-resource (:resource %)
                                                source-resource (:resource build-resource)]
                                            [source-resource build-resource])
                                         dep-build-targets))
            built-resources (into [[[:script] (when script (deps-by-source script))]]
                                  (map-indexed (fn [i {:keys [material] :as named-material}]
                                                 [[:materials i :material]
                                                  (when material (deps-by-source material))])
                                               named-materials))]
        [{:node-id _node-id
          :resource (workspace/make-build-resource resource)
          :build-fn build-render
          :user-data {:pb-msg pb-msg
                      :built-resources built-resources}
          :deps dep-build-targets}])))

(g/defnode RenderNode
  (inherits core/Scope)
  (inherits resource-node/ResourceNode)

  (property script resource/Resource
            (dynamic visible (g/constantly false))
            (value (gu/passthrough script-resource))
            (set (fn [_evaluation-context self old-value new-value]
                   (project/resource-setter self old-value new-value
                                            [:resource :script-resource]
                                            [:build-targets :dep-build-targets]))))
  (input script-resource resource/Resource)

  (input named-materials g/Any :array)
  (input dep-build-targets g/Any :array)

  (output form-data g/Any :cached produce-form-data)
  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output build-targets g/Any :cached produce-build-targets))

(defn- load-render [project self resource render-ddf]
  (let [graph-id (g/node-id->graph-id self)
        {script-path :script materials :materials} render-ddf]
    (concat
      (g/set-property self :script (workspace/resolve-resource resource script-path))
      (for [{:keys [name material]} materials]
        (let [material-resource (workspace/resolve-resource resource material)]
          (make-named-material-node graph-id self name material-resource))))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "render"
    :node-type RenderNode
    :ddf-type Render$RenderPrototypeDesc
    :load-fn load-render
    :icon "icons/32/Icons_30-Render.png"
    :view-types [:form-view :text]
    :label "Render"))
