(ns editor.settings
  (:require [clojure.string :as string]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [util.murmur :as murmur]
            [editor.graph-util :as gu]
            [editor.core :as core]
            [editor.settings-core :as settings-core]
            [editor.defold-project :as project]
            [camel-snake-kebab :as camel]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [service.log :as log]))

(g/defnode ResourceSettingNode
  (property resource-connections g/Any) ; [target-node-id [connections]]
  (property path g/Any)
  (property value resource/Resource
            (dynamic visible (g/constantly false))
            (value (gu/passthrough resource))
            (set (fn [evaluation-context self old-value new-value]
                   (concat
                     ;; connect resource node to this
                     (project/resource-setter evaluation-context self old-value new-value [:resource :resource])
                     (when-let [resource-connections (g/node-value self :resource-connections evaluation-context)]
                       (let [[target-node connections] resource-connections]
                         ;; connect extra resource node outputs directly to target-node (GameProjectNode for instance)
                         (apply project/resource-setter evaluation-context target-node old-value new-value
                           connections)))))))
  (input resource resource/Resource)
  ;; resource-setting-reference only consumed by SettingsNode and already cached there.
  (output resource-setting-reference g/Any (g/fnk [_node-id path value] {:path path :node-id _node-id :value value})))

(g/defnk produce-settings-map [meta-info raw-settings resource-settings]
  (let [meta-settings (:settings meta-info)
        sanitized-settings (settings-core/sanitize-settings meta-settings (settings-core/settings-with-value raw-settings))
        all-settings (concat (settings-core/make-default-settings meta-settings) sanitized-settings resource-settings)
        settings-map (settings-core/make-settings-map all-settings)]
    settings-map))

(defn- set-raw-setting [settings {:keys [path] :as meta-setting} value]
  (settings-core/set-setting settings path (settings-core/render-raw-setting-value meta-setting value)))

(defn- set-form-op [{:keys [node-id resource-setting-nodes meta-settings]} path value]
  (let [meta-setting (settings-core/get-meta-setting meta-settings path)]
    (if-let [resource-setting-node-id (resource-setting-nodes path)]
      (do
        (g/set-property! resource-setting-node-id :value value)
        (g/update-property! node-id :raw-settings set-raw-setting meta-setting (resource/resource->proj-path value)))
      (g/update-property! node-id :raw-settings set-raw-setting meta-setting value))))

(defn- clear-form-op [{:keys [node-id resource-setting-nodes meta-settings]} path]
  (when-let [resource-setting-node-id (resource-setting-nodes path)]
    (let [default-resource (settings-core/get-default-setting meta-settings path)]
      (g/set-property! resource-setting-node-id :value default-resource)))
  (g/update-property! node-id :raw-settings settings-core/clear-setting path))

(defn- make-form-ops [node-id resource-setting-nodes meta-settings]
  {:user-data {:node-id node-id :resource-setting-nodes resource-setting-nodes :meta-settings meta-settings}
   :set set-form-op
   :clear clear-form-op})

(defn- make-form-values-map [settings]
  (settings-core/make-settings-map settings))

(defn- label [key]
  (-> key
      name
      camel/->Camel_Snake_Case_String
      (string/replace "_" " ")))

(defn- make-form-field [setting]
  (cond-> (assoc setting
                 :label (or (:label setting) (label (second (:path setting))))
                 :optional true
                 :hidden? (:hidden? setting false))

    (contains? setting :options)
    (assoc :type :choicebox)

    (= :library-list (:type setting))
    (assoc :type :list :element {:type :url :default "http://url.to/library"})))

(defn- make-form-section [category-name category-info settings]
  {:title (or (:title category-info) category-name)
   :help (:help category-info)
   :fields (mapv make-form-field settings)})

(defn- make-form-data [form-ops meta-info settings]
  (let [meta-settings (:settings meta-info)
        categories (distinct (mapv settings-core/presentation-category meta-settings))
        category->settings (group-by settings-core/presentation-category meta-settings)
        sections (mapv #(make-form-section % (get-in meta-info [:categories %]) (category->settings %)) categories)
        values (make-form-values-map settings)]
    {:form-ops form-ops :sections sections :values values}))

(g/defnk produce-form-data [_node-id meta-info raw-settings resource-setting-nodes resource-settings]
  (let [meta-settings (:settings meta-info)
        sanitized-settings (settings-core/sanitize-settings meta-settings (settings-core/settings-with-value raw-settings))
        non-defaulted-setting-paths (into #{} (map :path (filter :value sanitized-settings)))
        non-default-resource-settings (filter (comp non-defaulted-setting-paths :path) resource-settings)
        all-settings (concat sanitized-settings non-default-resource-settings)]
    (make-form-data (make-form-ops _node-id resource-setting-nodes meta-settings) meta-info all-settings)))

(g/defnode SettingsNode
  (inherits core/Scope) ;; not a resource node, but a scope node for ResourceSettingNode's

  (property raw-settings g/Any (dynamic visible (g/constantly false)))
  (property meta-info g/Any (dynamic visible (g/constantly false)))

  (input resource-setting-references g/Any :array)

  (output resource-setting-nodes g/Any :cached
          (g/fnk [resource-setting-references]
                 (into {} (map (juxt :path :node-id) resource-setting-references))))
  (output resource-settings g/Any :cached
          (g/fnk [resource-setting-references]
                 (map #(select-keys % [:path :value]) resource-setting-references)))

  (output merged-raw-settings g/Any :cached
          (g/fnk [raw-settings meta-info resource-settings]
                 (let [meta-settings-map (settings-core/make-meta-settings-map (:settings meta-info))
                       raw-resource-settings (mapv (fn [setting]
                                                    (update setting :value
                                                            #(when %
                                                               (settings-core/render-raw-setting-value (meta-settings-map (:path setting))
                                                                                                (resource/resource->proj-path %)))))
                                                  resource-settings)
                       meta-settings (:meta-settings meta-info)]
                   (reduce (fn [raw {:keys [path value]}]
                             (if (settings-core/get-setting raw path)
                               (settings-core/set-setting raw path value)
                               raw))
                           raw-settings
                           raw-resource-settings))))

  (output settings-map g/Any :cached produce-settings-map)
  (output form-data g/Any :cached produce-form-data)

  (output save-value g/Any (gu/passthrough merged-raw-settings)))

(defn- resolve-resource-settings [settings base-resource value-field]
  (mapv (fn [setting]
          (if (= :resource (:type setting))
            (update setting value-field #(workspace/resolve-resource base-resource %))
            setting))
        settings))

(defn- type-annotate-settings [settings meta-settings]
  (let [meta-settings-map (settings-core/make-meta-settings-map meta-settings)]
    (mapv #(assoc % :type (:type (meta-settings-map (:path %)))) settings)))

(defn load-settings-node [self resource raw-settings initial-meta-info resource-setting-connections]
  (let [meta-info (-> (settings-core/add-meta-info-for-unknown-settings initial-meta-info raw-settings)
                      (update :settings resolve-resource-settings resource :default))
        meta-settings (:settings meta-info)
        settings (-> (settings-core/sanitize-settings meta-settings raw-settings) ; this provokes parse errors if any
                     (type-annotate-settings meta-settings)
                     (resolve-resource-settings resource :value))
        resource-setting-paths (set (map :path (filter #(= :resource (:type %)) meta-settings)))
        graph-id (g/node-id->graph-id self)]

    (concat
      ;; We retain the actual raw string settings and update these when/if the user changes a setting,
      ;; rather than parse to proper typed/sanitized values and rendering these on save - again only to
      ;; reduce diffs.
      ;; Resource settings are also stored in separate ResourceSettingNodes, and this is the canonical place to look
      ;; for the settings current value. The corresponding setting in raw-settings may be stale and is only used to track
      ;; whether the value is/was set or cleared.
      (g/set-property self :raw-settings raw-settings :meta-info meta-info)
      (for [resource-setting-path resource-setting-paths]
        (let [resource (settings-core/get-setting-or-default meta-settings settings resource-setting-path)]
          (g/make-nodes graph-id [resource-setting-node [ResourceSettingNode :path resource-setting-path :resource-connections (resource-setting-connections resource-setting-path)]]
                        (g/connect resource-setting-node :_node-id self :nodes)
                        (when resource
                          (g/set-property resource-setting-node :value resource))
                        (g/connect resource-setting-node :resource-setting-reference self :resource-setting-references)))))))
