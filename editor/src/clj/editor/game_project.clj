;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.game-project
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.code.lang.ini :as ini]
            [editor.form :as form]
            [editor.fs :as fs]
            [editor.game-project-core :as gpcore]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.settings :as settings]
            [editor.settings-core :as settings-core]
            [editor.workspace :as workspace]
            [util.murmur :as murmur])
  (:import [org.apache.commons.io IOUtils]))

(set! *warn-on-reflection* true)

(def game-project-icon "icons/32/Icons_04-Project-file.png")

(defn- ignored-setting?
  [{:keys [path]}]
  (= path ["project" "dependencies"]))

;; Transform a settings map with build-time settings conversions.
(defn- transform-settings! [settings]
  ;; Map deprecated 'variable_dt' to new values for same runtime behavior
  (if (= true (get settings ["display", "variable_dt"] false))
    (-> settings
        (assoc  ["display", "vsync"] false)
        (assoc  ["display", "update_frequency"] 0))
    settings))

(defn- build-game-project [resource dep-resources user-data]
  (let [{:keys [settings-map meta-settings path->built-resource-settings]} user-data
        settings (into []
                       (comp (keep (fn [[path value]]
                                     (if (:unknown-setting? (settings-core/get-meta-setting meta-settings path))
                                       {:path path :value value}
                                       (when (and (some? value) (not= "" value))
                                         {:path path :value value}))))
                             (remove ignored-setting?)
                             (keep (fn [{:keys [path value] :as setting}]
                                     (let [meta-setting (settings-core/get-meta-setting meta-settings path)]
                                       (if (= :resource (:type meta-setting))
                                         (if-some [resource-value (path->built-resource-settings path)]
                                           (let [build-resource-path (resource/proj-path (dep-resources resource-value))]
                                             (assoc setting :value build-resource-path))
                                           (assoc setting :value (resource/proj-path value)))
                                         (assoc setting :value (settings-core/render-raw-setting-value meta-setting value)))))))
                       (let [transformed-settings-map (transform-settings! settings-map)]
                         (sort-by first transformed-settings-map)))
        user-data-content (settings-core/settings->str settings meta-settings :comma-separated-list)]
    {:resource resource :content (.getBytes user-data-content)}))

(defn- resource-content [resource]
  (with-open [s (io/input-stream resource)]
    (IOUtils/toByteArray s)))

(defn- build-custom-resource [resource dep-resources user-data]
  {:resource resource :content (resource-content (:resource resource))})

(defrecord CustomResource [resource]
  ;; Only purpose is to provide resource-type with :build-ext = :ext
  resource/Resource
  (children [this] (resource/children resource))
  (ext [this] (resource/ext resource))
  (resource-type [this]
    (let [ext (resource/ext this)]
      {:ext ext
       :label (localization/message "resource.type.custom")
       :build-ext ext}))
  (source-type [this] (resource/source-type resource))
  (exists? [this] (resource/exists? resource))
  (read-only? [this] (resource/read-only? resource))
  (path [this] (resource/path resource))
  (abs-path [this] (resource/abs-path resource))
  (proj-path [this] (resource/proj-path resource))
  (resource-name [this] (resource/resource-name resource))
  (workspace [this] (resource/workspace resource))
  (resource-hash [this] (resource/resource-hash resource))
  (openable? [this] (resource/openable? resource))
  (editable? [this] (resource/editable? resource))
  (loaded? [this] (resource/loaded? resource))

  io/IOFactory
  (make-input-stream  [this opts] (io/input-stream resource))
  (make-reader        [this opts] (io/reader resource))
  (make-output-stream [this opts] (io/output-stream resource))
  (make-writer        [this opts] (io/writer resource)))

(defn- make-custom-build-target [node-id resource]
  (bt/with-content-hash
    {:node-id node-id
     :resource (workspace/make-build-resource (CustomResource. resource))
     :build-fn build-custom-resource
     ;; NOTE! Break build cache when resource content changes.
     :user-data {:hash (murmur/hash64-bytes (resource-content resource))}}))

(defn- strip-trailing-slash [path]
  (string/replace path #"/*$" ""))

(defn- file-resource? [resource]
  (= (resource/source-type resource) :file))

(defn- find-custom-resources [resource-map custom-paths]
  (->> (flatten (keep (fn [custom-path]
                        (let [base-resource (resource-map custom-path)]
                          (if base-resource
                            (resource/resource-seq base-resource)
                            (throw (ex-info (format "Custom resource not found: '%s'" custom-path)
                                            {})))))
                      custom-paths))
       (distinct)
       (filter file-resource?)))

(defn- parse-custom-resource-paths [cr-setting]
  (let [paths (remove string/blank? (map string/trim (string/split (or cr-setting "")  #",")))]
    (map (comp strip-trailing-slash fs/with-leading-slash) paths)))

(def ^:private resource-setting-connections-template
  {["display" "display_profiles"] [[:build-targets :dep-build-targets]
                                   [:profile-data :display-profiles-data]]
   ["bootstrap" "debug_init_script"] [[:build-targets :dep-build-targets]]
   ["bootstrap" "main_collection"] [[:build-targets :dep-build-targets]]
   ["bootstrap" "render"] [[:build-targets :dep-build-targets]]
   ["graphics" "texture_profiles"] [[:build-targets :dep-build-targets]
                                    [:pb :texture-profiles-data]]
   ["input" "gamepads"] [[:build-targets :dep-build-targets]]
   ["input" "game_binding"] [[:build-targets :dep-build-targets]]})

(g/defnk produce-build-targets [_node-id build-errors resource settings-map meta-info custom-build-targets resource-settings dep-build-targets]
  (g/precluding-errors (some-> (g/flatten-errors build-errors) (assoc :_node-id _node-id))
     (let [clean-meta-info (settings-core/remove-to-from-string meta-info)
           dep-build-targets (vec (into (flatten dep-build-targets) custom-build-targets))
           deps-by-source (into {} (map
                                     (fn [build-target]
                                       (let [build-resource (:resource build-target)
                                             source-resource (:resource build-resource)]
                                         [source-resource build-resource]))
                                     dep-build-targets))
           path->built-resource-settings (into {} (keep (fn [resource-setting]
                                                          (when (resource-setting-connections-template (:path resource-setting))
                                                            [(:path resource-setting) (deps-by-source (:value resource-setting))]))
                                                        resource-settings))]
       [(bt/with-content-hash
          {:node-id _node-id
           :resource (workspace/make-build-resource resource)
           :build-fn build-game-project
           :user-data {:settings-map settings-map
                       :meta-settings (:settings clean-meta-info)
                       :path->built-resource-settings path->built-resource-settings}
           :deps dep-build-targets})])))

(g/defnode GameProjectNode
  (inherits resource-node/ResourceNode)

  (input display-profiles-data g/Any)
  (output display-profiles-data g/Any (gu/passthrough display-profiles-data))

  (input texture-profiles-data g/Any)
  (output texture-profiles-data g/Any (gu/passthrough texture-profiles-data))

  (input settings-map g/Any)
  ;; settings-map already cached in SettingsNode
  (output settings-map g/Any (gu/passthrough settings-map))

  (input form-data g/Any)
  (output form-data g/Any :cached (gu/passthrough form-data))

  (input raw-settings g/Any)
  (input resource-settings g/Any)

  (input resource-map g/Any)
  (input dep-build-targets g/Any :array)
  (input meta-info g/Any)

  (input build-errors g/Any :array)

  (output custom-build-targets g/Any :cached
          (g/fnk [_node-id resource-map settings-map]
                 (let [custom-resources (parse-custom-resource-paths (get settings-map ["project" "custom_resources"]))
                       ssl-certificates (get settings-map ["network" "ssl_certificates"])
                       custom-paths (if (some? ssl-certificates)
                                      (conj custom-resources (resource/proj-path ssl-certificates))
                                      custom-resources)]
                   (try
                     (map (partial make-custom-build-target _node-id)
                          (find-custom-resources resource-map custom-paths))
                     (catch Throwable error
                       (g/map->error
                        {:_node-id _node-id
                         :_label :custom-build-targets
                         :message (ex-message error)
                         :severity :fatal}))))))

  (input save-value g/Any)
  (output save-value g/Any (gu/passthrough save-value))

  (output build-targets g/Any :cached produce-build-targets))

;;; loading node

(defn- load-game-project [project self resource source-value]
  (let [graph-id (g/node-id->graph-id self)
        resource-setting-connections (reduce-kv (fn [m k v] (assoc m k [self v])) {} resource-setting-connections-template)]
    (concat
      (g/make-nodes graph-id [settings-node settings/SettingsNode]
                    (g/connect settings-node :_node-id self :nodes)
                    (g/connect settings-node :settings-map self :settings-map)
                    (g/connect settings-node :save-value self :save-value)
                    (g/connect settings-node :form-data self :form-data)
                    (g/connect settings-node :raw-settings self :raw-settings)
                    (g/connect settings-node :meta-info self :meta-info)
                    (g/connect settings-node :resource-settings self :resource-settings)
                    (g/connect settings-node :setting-errors self :build-errors)
                    (settings/load-settings-node project self settings-node resource source-value gpcore/basic-meta-info resource-setting-connections))
      (g/connect project :resource-map self :resource-map))))

;; Test support

(defn set-setting!
  "Exposed for tests"
  [game-project path value]
  (g/transact (form/set-value (:form-ops (g/node-value game-project :form-data)) path value)))

(defn get-setting
  ([game-project path]
   (g/with-auto-evaluation-context evaluation-context
     (get-setting game-project path evaluation-context)))
  ([game-project path evaluation-context]
   ((g/node-value game-project :settings-map evaluation-context) path)))

(defn register-resource-types [workspace]
  (resource-node/register-settings-resource-type workspace
    :ext "project"
    :label (localization/message "resource.type.project")
    :node-type GameProjectNode
    :load-fn load-game-project
    :meta-settings (:settings gpcore/basic-meta-info)
    :icon game-project-icon
    :icon-class :property
    :view-types [:cljfx-form-view :text]
    :language "ini"
    :view-opts {:text {:grammar ini/grammar}}))
