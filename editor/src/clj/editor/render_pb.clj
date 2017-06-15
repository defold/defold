(ns editor.render-pb
  (:require
    [dynamo.graph :as g]
    [editor.graph-util :as gu]
    [editor.resource :as resource]
    [editor.resource-node :as resource-node]
    [editor.workspace :as workspace]
    [editor.protobuf :as protobuf]
    [editor.defold-project :as project])
  (:import
    [com.dynamo.render.proto Render$RenderPrototypeDesc]))

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
       :path [:materials]
       :type :table
       :label "Materials"
       :columns [{:path [:name] :label "Name" :type :string :default "New Material"}
                 {:path [:material] :label "Material" :type :resource :filter "material" :default nil}]
       }
      ]
     }
    ]
   })

(defn- set-form-op [{:keys [node-id]} path value]
  (condp = path
    [:script] (g/set-property! node-id :script value)
    [:materials] (let [names (mapv :name value)
                       materials (mapv :material value)]
                    (g/set-property! node-id :materials-name names)
                    (g/set-property! node-id :materials-material materials))))

(g/defnk produce-form-data [_node-id script-resource materials-name materials-material]
  (-> form-sections
      (assoc :form-ops {:user-data {:node-id _node-id}
                        :set set-form-op})
      (assoc :values {[:script] script-resource
                      [:materials] (mapv (fn [name material] {:name name :material material}) materials-name materials-material)})))

(g/defnk produce-pb-msg [script-resource materials-name materials-material]
  {:script (resource/resource->proj-path script-resource)
   :materials (mapv (fn [name material] {:name name :material (resource/resource->proj-path material)}) materials-name materials-material)})

(defn- build-render [self basis resource dep-resources user-data]
  (let [{:keys [pb-msg built-resources]} user-data
        built-pb (reduce (fn [pb [path built-resource]]
                           (assoc-in pb path (resource/proj-path (get dep-resources built-resource))))
                         pb-msg
                         built-resources)]
    {:resource resource :content (protobuf/map->bytes Render$RenderPrototypeDesc built-pb)}))

(g/defnk produce-build-targets [_node-id resource pb-msg dep-build-targets script materials-material]
  (let [dep-build-targets (flatten dep-build-targets)
        deps-by-source (into {} (map #(let [build-resource (:resource %)
                                            source-resource (:resource build-resource)]
                                        [source-resource build-resource])
                                     dep-build-targets))
        built-resources (into [[[:script] (when script (deps-by-source script))]]
                              (map-indexed (fn [i material] [[:materials i :material] (when material (deps-by-source material))])
                                           materials-material))]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-render
      :user-data {:pb-msg pb-msg
                  :built-resources built-resources}
      :deps dep-build-targets}]))

(g/defnode RenderNode
  (inherits resource-node/ResourceNode)

  (property script resource/Resource
            (dynamic visible (g/constantly false))
            (value (gu/passthrough script-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                            [:resource :script-resource]
                                            [:build-targets :dep-build-targets]))))
  (input script-resource resource/Resource)

  (property materials-name g/Any
            (dynamic visible (g/constantly false)))
  
  (property materials-material resource/ResourceVec
            (dynamic visible (g/constantly false))
            (value (gu/passthrough material-resources))
            (set (fn [basis self old-value new-value]
                   (let [project (project/get-project self)
                         connections [[:resource :material-resources]
                                      [:build-targets :dep-build-targets]]]
                     (concat
                       (for [r old-value]
                         (if r
                           (project/disconnect-resource-node project r self connections)
                           (g/disconnect project :nil-resource self :material-resources)))
                       (for [r new-value]
                         (if r
                           (project/connect-resource-node project r self connections)
                           (g/connect project :nil-resource self :material-resources))))))))

  (input material-resources resource/Resource :array)
  (input dep-build-targets g/Any :array)

  (output form-data g/Any :cached produce-form-data)
  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output build-targets g/Any :cached produce-build-targets))

(defn- load-render [project self resource render-ddf]
  (let [{script-path :script
         materials :materials} render-ddf]
    (concat
      (g/set-property self :script (workspace/resolve-resource resource script-path))
      (g/set-property self :materials-name (mapv :name materials))
      (g/set-property self :materials-material (mapv #(workspace/resolve-resource resource %) (map :material materials))))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "render"
    :node-type RenderNode
    :ddf-type Render$RenderPrototypeDesc
    :load-fn load-render
    :icon "icons/32/Icons_30-Render.png"
    :view-types [:form-view :text]
    :label "Render"))
