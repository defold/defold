;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.settings
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.settings-core :as settings-core]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [util.coll :as coll]
            [util.eduction :as e]))

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

(defn- resolve-resource-settings-from-raw [raw-settings meta-settings owner-resource evaluation-context]
  ;; evaluation context is an `^:unsafe` part of the output: can be used only
  ;; for resource resolution (that are then only needed for paths)
  (let [meta-settings-map (settings-core/make-meta-settings-map meta-settings)]
    (->> raw-settings
         (settings-core/settings-with-value)
         (settings-core/sanitize-settings meta-settings)
         (mapv (fn [{:keys [path value] :as setting}]
                 ;; We resolve raw string values to resources so the form gets
                 ;; typed values; this may happen when a :resource setting is
                 ;; defined via, e.g., game.properties, without a corresponding
                 ;; ResourceSettingNode in the graph.
                 (cond-> setting
                         (and (string? value) (= :resource (:type (meta-settings-map path))))
                         (assoc :value (workspace/resolve-resource owner-resource value evaluation-context))))))))

(g/defnk produce-settings-map [^:unsafe _evaluation-context owner-resource meta-info raw-settings resource-settings]
  ;; we use evaluation context to resolve a resource; we only need the resource
  ;; for its path, so it's safe to use it here
  (let [meta-settings (:settings meta-info)
        sanitized-settings (resolve-resource-settings-from-raw raw-settings meta-settings owner-resource _evaluation-context)
        all-settings (concat (settings-core/make-default-settings meta-settings) sanitized-settings resource-settings)
        settings-map (settings-core/make-settings-map all-settings)]
    settings-map))

(defn- set-raw-setting [settings {:keys [path] :as meta-setting} value]
  (settings-core/set-setting settings path (settings-core/render-raw-setting-value meta-setting value)))

(defn- make-resource-setting-node [self resource path resource-setting-connections]
  (g/make-nodes (g/node-id->graph-id self) [resource-setting-node [ResourceSettingNode :path path :resource-connections (resource-setting-connections path)]]
    (g/connect resource-setting-node :_node-id self :nodes)
    (when resource
      (g/set-property resource-setting-node :value resource))
    (g/connect resource-setting-node :resource-setting-reference self :resource-setting-references)))

(defn set-tx-data [{:keys [node-id resource-setting-nodes meta-settings resource-setting-connections] :as _user-data} path value]
  (let [meta-setting (settings-core/get-meta-setting meta-settings path)]
    (case (:type meta-setting)
      :resource
      (concat
        (if-let [resource-setting-node-id (resource-setting-nodes path)]
          (g/set-property resource-setting-node-id :value value)
          (make-resource-setting-node node-id value (:path meta-setting) resource-setting-connections))
        (g/update-property node-id :raw-settings set-raw-setting meta-setting (resource/resource->proj-path value)))

      (g/update-property node-id :raw-settings set-raw-setting meta-setting value))))

(defn- set-form-op [{:keys [project owner-resource] :as user-data} path value]
  (concat
    (set-tx-data user-data path value)
    (when (and (= "/game.project" (resource/proj-path owner-resource))
               (= path ["project" "dependencies"]))
      (g/expand-ec
        (fn update-fetch-libraries-notification [evaluation-context]
          (project/update-fetch-libraries-notification project evaluation-context))))))

(defn clear-tx-data [{:keys [node-id resource-setting-nodes meta-settings] :as _user-data} path]
  (concat
    (when-some [resource-setting-node-id (resource-setting-nodes path)]
      (let [default-resource (settings-core/get-default-setting meta-settings path)]
        (g/set-property resource-setting-node-id :value default-resource)))
    (g/update-property node-id :raw-settings settings-core/clear-setting path)))

(defn- clear-form-op [user-data path]
  (clear-tx-data user-data path))

(defn- make-form-values-map [settings]
  (settings-core/make-settings-map settings))

(defn- make-form-field [setting localization-prefix]
  (cond-> (assoc setting
            :label (or (:label setting) (settings-core/label (second (:path setting))))
            :localization-key (cond->> (coll/join-to-string "." (:path setting)) localization-prefix (str localization-prefix "."))
            :optional true
            :hidden (:hidden setting false))

    (contains? setting :options)
    (assoc :type :choicebox)

    (= :comma-separated-list (:type setting))
    (assoc :type :list)))

(defn- section-title [category-name category-info]
  (or (:title category-info)
      (settings-core/label category-name)))

(defn- make-form-section [localization-prefix category-name category-info settings]
  {:title (section-title category-name category-info)
   :localization-key (cond->> category-name localization-prefix (str localization-prefix "."))
   :group (:group category-info "Other")
   :help (:help category-info)
   :fields (mapv #(make-form-field % localization-prefix) settings)})

(defn- make-form-data [form-ops meta-info settings]
  (let [{meta-settings :settings meta-categories :categories :keys [localization-prefix]} meta-info
        categories (distinct (mapv settings-core/presentation-category meta-settings))
        category->settings (group-by settings-core/presentation-category meta-settings)
        sections (mapv #(make-form-section localization-prefix % (get meta-categories %) (category->settings %)) categories)
        values (make-form-values-map settings)
        group-order (into {}
                          (map-indexed (fn [i v]
                                         [v i]))
                          (:group-order meta-info))]
    {:form-ops form-ops
     :sections sections
     :values values
     :meta-settings meta-settings
     :group-order group-order
     :default-section-name (when-let [category-name (:default-category meta-info)]
                             (section-title category-name (get-in meta-info [:categories category-name])))}))

(defn get-setting-build-error [setting-value meta-setting label]
  (if (and (some? setting-value))
    (let [max-error (when (some? (:maximum meta-setting))
                      (validation/prop-maximum-check? (:maximum meta-setting) setting-value (string/join "." (:path meta-setting))))
          min-error (when (some? (:minimum meta-setting))
                      (validation/prop-minimum-check? (:minimum meta-setting) setting-value (string/join "." (:path meta-setting))))]
      (cond
        (some? max-error) (g/->error nil label :fatal nil max-error)
        (some? min-error) (g/->error nil label :fatal nil min-error)))))

(defn get-setting-error [setting-value meta-setting label]
  (cond
    (and (some? setting-value)
         (= :resource (:type meta-setting))
         (not (resource/exists? setting-value)))
    (g/->error nil label :fatal nil (:help meta-setting))

    (and (some? setting-value) (:deprecated meta-setting))
    (if (not= setting-value (:default meta-setting))
      (g/->error nil label (:severity-override meta-setting) nil (:help meta-setting))
      (g/->error nil label (:severity-default meta-setting) nil (:help meta-setting)))

    :else
    (get-setting-build-error setting-value meta-setting label)))

(defn get-settings-errors [form-data]
  (let [meta-settings (:meta-settings form-data)
        setting-values (:values form-data)]
    (into []
          (keep (fn [[setting-path setting-value]]
                  (let [meta-setting (settings-core/get-meta-setting meta-settings setting-path)]
                    (when-some [error (get-setting-build-error setting-value meta-setting :build-targets)]
                      error))))
          setting-values)))

(defn get-dependencies-setting-errors-from
  "Return build errors derived from a dependency vector (e.g., incompatible
  library.defold_min_version)."
  [dependencies]
  (into []
        (comp
          (filter #(and (= :error (:status %))
                        (= :defold-min-version (:reason %))))
          (map (fn [{:keys [uri required current]}]
                 (g/map->error
                   {:severity :fatal
                    :message (format "Library '%s' requires Defold %s or newer (current Defold version is %s). Update Defold or check older extension versions for compatibility."
                                     (str uri) (str required) (str current))}))))
        dependencies))

(g/defnk produce-form-data [^:unsafe _evaluation-context _node-id project owner-resource meta-info raw-settings resource-setting-nodes resource-settings resource-setting-connections]
  ;; we use evaluation context to resolve a resource; we only need the resource
  ;; for its path, so it's safe to use it here
  (let [meta-settings (:settings meta-info)
        sanitized-settings (resolve-resource-settings-from-raw raw-settings meta-settings owner-resource _evaluation-context)
        non-defaulted-setting-paths (into #{}
                                          (comp
                                            (filter :value)
                                            (map :path))
                                          sanitized-settings)
        non-default-resource-settings (filterv (comp non-defaulted-setting-paths :path) resource-settings)
        all-settings (e/concat sanitized-settings non-default-resource-settings)]
    (make-form-data
      {:user-data {:node-id _node-id
                   :project project
                   :owner-resource owner-resource
                   :resource-setting-nodes resource-setting-nodes
                   :meta-settings meta-settings
                   :resource-setting-connections resource-setting-connections}
       :set set-form-op
       :clear clear-form-op}
      meta-info
      all-settings)))

(g/defnode SettingsNode
  (inherits core/Scope) ;; not a resource node, but a scope node for ResourceSettingNode's

  (property raw-settings g/Any (dynamic visible (g/constantly false)))
  (property raw-meta-info g/Any (dynamic visible (g/constantly false)))
  (property resource-setting-connections g/Any (dynamic visible (g/constantly false)))

  (input resource-setting-references g/Any :array)
  (input project g/NodeID)
  (input owner-resource resource/Resource)
  (input meta-infos g/Any)
  (input project-dependencies g/Any)

  (output resource-setting-references g/Any :cached
          (g/fnk [resource-setting-references meta-info]
            (let [meta-settings (:settings meta-info)
                  meta-settings-map (settings-core/make-meta-settings-map meta-settings)]
              ;; We filter out nodes for settings that were resource, but stopped
              ;; being them; which may happen when we define a resource setting
              ;; using, e.g., game.properties file, then set the property, and
              ;; then we change the type to be a string
              (filterv #(= :resource (:type (meta-settings-map (:path %)))) resource-setting-references))))
  (output resource-setting-nodes g/Any :cached
          (g/fnk [resource-setting-references]
            (into {} (map (juxt :path :node-id) resource-setting-references))))
  (output resource-settings g/Any :cached
          (g/fnk [resource-setting-references]
            (mapv #(select-keys % [:path :value]) resource-setting-references)))

  (output merged-raw-settings g/Any :cached
          (g/fnk [raw-settings meta-info resource-settings]
                 (let [meta-settings-map (settings-core/make-meta-settings-map (:settings meta-info))
                       raw-resource-settings (mapv (fn [setting]
                                                    (update setting :value
                                                            #(when %
                                                               (settings-core/render-raw-setting-value
                                                                 (meta-settings-map (:path setting))
                                                                 (resource/resource->proj-path %)))))
                                                  resource-settings)]
                   (reduce (fn [raw {:keys [path value]}]
                             (if (settings-core/get-setting raw path)
                               (settings-core/set-setting raw path value)
                               raw))
                           raw-settings
                           raw-resource-settings))))

  (output settings-map g/Any :cached produce-settings-map)
  (output form-data g/Any :cached produce-form-data)

  (output save-value g/Any (gu/passthrough merged-raw-settings))
  (output setting-errors g/Any :cached (g/fnk [form-data project-dependencies]
                                         (let [base-errors (get-settings-errors form-data)]
                                           (into base-errors (get-dependencies-setting-errors-from project-dependencies)))))
  (output meta-info g/Any :cached
          (g/fnk [raw-meta-info meta-infos owner-resource]
            (let [{:keys [ext-meta-info game-project-proj-path->additional-meta-info]} meta-infos
                  project-meta-info (game-project-proj-path->additional-meta-info (resource/proj-path owner-resource))]
              (cond-> raw-meta-info
                      project-meta-info (settings-core/merge-meta-infos project-meta-info)
                      (and ext-meta-info (= "project" (resource/type-ext owner-resource))) (settings-core/merge-meta-infos ext-meta-info))))))

(defn- resolve-resource-settings [settings base-resource value-field]
  (mapv (fn [setting]
          (if (= :resource (:type setting))
            (update setting value-field #(workspace/resolve-resource base-resource %))
            setting))
        settings))

(defn- type-annotate-settings [settings meta-settings]
  (let [meta-settings-map (settings-core/make-meta-settings-map meta-settings)]
    (mapv #(assoc % :type (:type (meta-settings-map (:path %)))) settings)))

(defn load-settings-node [project owner-resource-node self resource raw-settings initial-meta-info resource-setting-connections]
  (let [meta-info (-> (settings-core/add-meta-info-for-unknown-settings initial-meta-info raw-settings)
                      (update :settings resolve-resource-settings resource :default))
        meta-settings (:settings meta-info)
        settings (-> (settings-core/sanitize-settings meta-settings raw-settings) ; this provokes parse errors if any
                     (type-annotate-settings meta-settings)
                     (resolve-resource-settings resource :value))
        resource-setting-paths (set (map :path (filter #(= :resource (:type %)) meta-settings)))]
    (concat
      ;; We retain the actual raw string settings and update these when/if the user changes a setting,
      ;; rather than parse to proper typed/sanitized values and rendering these on save - again only to
      ;; reduce diffs.
      ;; Resource settings are also stored in separate ResourceSettingNodes, and this is the canonical place to look
      ;; for the settings current value. The corresponding setting in raw-settings may be stale and is only used to track
      ;; whether the value is/was set or cleared.
      (g/set-properties self :raw-settings raw-settings :raw-meta-info meta-info :resource-setting-connections resource-setting-connections)
      (g/connect project :meta-infos self :meta-infos)
      (g/connect project :_node-id self :project)
      (g/connect project :dependencies self :project-dependencies)
      (g/connect owner-resource-node :resource self :owner-resource)
      (for [path resource-setting-paths]
        (let [resource (settings-core/get-setting-or-default meta-settings settings path)]
          (make-resource-setting-node self resource path resource-setting-connections))))))

(g/defnode SimpleSettingsResourceNode
  (inherits resource-node/ResourceNode)

  (input form-data g/Any)
  (output form-data g/Any (gu/passthrough form-data))

  (input save-value g/Any)
  (output save-value g/Any (gu/passthrough save-value))

  (input settings-map g/Any)
  (output settings-map g/Any (gu/passthrough settings-map)))

(defn- load-simple-settings-resource-node [meta-info project self resource source-value]
  (let [graph-id (g/node-id->graph-id self)]
    (concat
      (g/make-nodes graph-id [settings-node SettingsNode]
        (g/connect settings-node :_node-id self :nodes)
        (g/connect settings-node :save-value self :save-value)
        (g/connect settings-node :form-data self :form-data)
        (g/connect settings-node :settings-map self :settings-map)
        (load-settings-node project self settings-node resource source-value meta-info nil)))))

(defn register-simple-settings-resource-type [workspace & {:keys [ext label icon meta-info]}]
  (resource-node/register-settings-resource-type workspace
    :ext ext
    :label label
    :icon icon
    :node-type SimpleSettingsResourceNode
    :load-fn (partial load-simple-settings-resource-node meta-info)
    :meta-settings (:settings meta-info)
    :view-types [:cljfx-form-view :text]))
