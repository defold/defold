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

(ns editor.game-project
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.code.lang.ini :as ini]
            [editor.form :as form]
            [editor.fs :as fs]
            [editor.game-project-core :as gpcore]
            [editor.gamepads :as gamepads]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.pipeline :as pipeline]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.settings :as settings]
            [editor.settings-core :as settings-core]
            [editor.workspace :as workspace]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce]
            [util.path :as path])
  (:import [com.dynamo.bob.util DependencyMetadata Library$Archive Library$Result]
           [com.fasterxml.jackson.databind ObjectMapper]
           [java.io ByteArrayInputStream ByteArrayOutputStream]))

(set! *warn-on-reflection* true)

(def game-project-icon "icons/32/Icons_04-Project-file.png")

(def ^:private dependencies-metadata-setting-path ["project" "dependencies_metadata"])
(def ^:private ^ObjectMapper dependency-metadata-object-mapper (ObjectMapper.))

(defn- ignored-setting?
  [{:keys [path]}]
  (or (= ["project" "dependencies"] path)
      (= ["input" "gamepad_database"] path)))

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
                                     (let [meta-setting (settings-core/get-meta-setting meta-settings path)]
                                       (when (or (:unknown-setting meta-setting)
                                                 (and (= :resource (:type meta-setting))
                                                      (nil? value)
                                                      (:default meta-setting))
                                                 (and (some? value) (not= "" value)))
                                         {:path path :value value}))))
                             (remove ignored-setting?)
                             (keep (fn [{:keys [path value] :as setting}]
                                     (let [meta-setting (settings-core/get-meta-setting meta-settings path)]
                                       (if (= :resource (:type meta-setting))
                                         (if-let [build-resource (path->built-resource-settings path)]
                                           (assoc setting :value (resource/proj-path (dep-resources build-resource)))
                                           (assoc setting :value (resource/resource->proj-path value)))
                                         (assoc setting :value (settings-core/render-raw-setting-value meta-setting value)))))))
                       (sort-by first (transform-settings! settings-map)))
        user-data-content (settings-core/settings->str settings meta-settings :comma-separated-list)]
    {:resource resource :content (.getBytes user-data-content)}))

(defn- build-dependency-metadata [resource _dep-resources user-data]
  {:resource resource
   :content (.writeValueAsBytes dependency-metadata-object-mapper
                                (DependencyMetadata/collect (:digest-ignored/dependencies user-data)))})

(defn- dependency-content-hash-data [dependencies]
  (into []
        (map (fn [^Library$Result dependency]
               (let [archive ^Library$Archive (.archive dependency)]
                 (cond-> {:uri (.uri dependency)
                          :problem (some-> (.problem dependency) str)}
                   archive
                   (assoc :archive {:path (str (.path archive))
                                    :size (path/byte-size (.path archive))
                                    :modified-time (path/last-modified-ms (.path archive))
                                    :base-dir (.baseDir archive)
                                    :include-dirs (.includeDirs archive)
                                    :zip-comment (.zipComment archive)})))))
        dependencies))

(defonce/record DependencyMetadataResource [workspace]
  resource/Resource
  (children [_] nil)
  (ext [_] "json")
  (resource-type [_]
    {:ext "json"
     :label (localization/message "resource.type.custom")
     :build-ext "json"})
  (source-type [_] :file)
  (exists? [_] true)
  (read-only? [_] false)
  (symlink? [_] false)
  (path [_] DependencyMetadata/OUTPUT_PATH)
  (abs-path [_] (.getAbsolutePath (io/file (workspace/project-directory workspace) DependencyMetadata/OUTPUT_PATH)))
  (proj-path [_] (str "/" DependencyMetadata/OUTPUT_PATH))
  (resource-name [_] DependencyMetadata/DATA_FILE_NAME)
  (workspace [_] workspace)
  (resource-hash [_] (hash DependencyMetadata/OUTPUT_PATH))
  (openable? [_] false)
  (editable? [_] false)
  (loaded? [_] true)

  io/IOFactory
  (make-input-stream [_ _opts] (ByteArrayInputStream. (byte-array 0)))
  (make-reader [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (make-output-stream [_ opts] (io/make-output-stream (ByteArrayOutputStream.) opts))
  (make-writer [this opts] (io/make-writer (io/make-output-stream this opts) opts)))

(defonce/record CustomResource [resource]
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
  (symlink? [this] (resource/symlink? resource))
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

(defn- strip-trailing-slash [path]
  (string/replace path #"/*$" ""))

(defn- file-resource? [resource]
  (= (resource/source-type resource) :file))

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
   ["input" "gamepads"] [[:build-targets :gamepads-build-targets]
                         [:resource :gamepads-resource]
                         [:pb :gamepads-pb]]
   ["input" "gamepad_database"] [[:resource :gamepad-database-resource]
                                 [:lines :gamepad-database-lines]]
   ["input" "game_binding"] [[:build-targets :dep-build-targets]]})

(g/defnk produce-build-targets [_node-id build-errors resource settings-map meta-info custom-build-targets resource-settings dep-build-targets dependencies gamepads-build-targets gamepads-resource gamepads-pb gamepad-database-resource gamepad-database-lines]
  (g/precluding-errors [(some-> (g/flatten-errors build-errors) (assoc :_node-id _node-id))
                        gamepads-pb
                        gamepad-database-lines
                        (when (and gamepad-database-resource
                                   (resource/exists? gamepad-database-resource)
                                   (not= "txt" (resource/ext gamepad-database-resource)))
                          (g/->error _node-id :build-targets :fatal gamepad-database-resource
                                     (localization/message "error.game-project.gamepad-database-must-be-txt")))]
    (let [gamepads-build-target (if (and gamepads-build-targets (not gamepad-database-resource))
                                  (peek gamepads-build-targets)
                                  (gamepads/make-build-target _node-id gamepads-resource gamepads-pb gamepad-database-resource gamepad-database-lines))
          dep-build-targets (cond-> (vec (into (flatten dep-build-targets) custom-build-targets))
                              gamepads-build-target (conj gamepads-build-target))
          deps-by-source (into {}
                               (map (fn [{build-resource :resource}]
                                      [(:resource build-resource) build-resource]))
                               dep-build-targets)
          path->built-resource-settings (cond-> (into {}
                                                      (keep (fn [{:keys [path value]}]
                                                              (when (resource-setting-connections-template path)
                                                                [path (deps-by-source value)])))
                                                      resource-settings)
                                          gamepads-build-target
                                          (assoc ["input" "gamepads"] (:resource gamepads-build-target)))
          game-project-build-target (bt/with-content-hash
                                      {:node-id _node-id
                                       :resource (workspace/make-build-resource resource)
                                       :build-fn build-game-project
                                       :user-data {:settings-map settings-map
                                                   :meta-settings (:settings meta-info)
                                                   :path->built-resource-settings path->built-resource-settings}
                                       :deps dep-build-targets})]
      (cond-> [game-project-build-target]
        (and (get settings-map dependencies-metadata-setting-path)
             (coll/not-empty dependencies))
        (conj (bt/with-content-hash
                {:node-id _node-id
                 :resource (workspace/make-build-resource
                              (->DependencyMetadataResource (resource/workspace resource)))
                 :build-fn build-dependency-metadata
                 :user-data {:digest-ignored/dependencies dependencies
                             :dependency-content-hash-data (dependency-content-hash-data dependencies)}}))))))

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

  (input resource-settings g/Any)

  (input gamepads-resource resource/Resource)
  (input gamepads-build-targets g/Any)
  (input gamepads-pb g/Any)
  (input gamepad-database-resource resource/Resource)
  (input gamepad-database-lines g/Any)

  (input resource-map g/Any)
  (input resource-snapshot g/Any)
  (input dep-build-targets g/Any :array)
  (input dependencies g/Any)
  (input meta-info g/Any)

  (input build-errors g/Any :array)

  (output ssl-certificates-directory-resource g/Any
          (g/fnk [_node-id settings-map]
            (let [directory-resource (get settings-map ["network" "ssl_certificates"])]
              (if (or (nil? directory-resource)
                      (resource/exists? directory-resource))
                directory-resource
                (g/map->error
                  {:_node-id _node-id
                   :severity :fatal
                   :message (format "SSL certificates directory not found: '%s'" (resource/proj-path directory-resource))})))))

  (output custom-resources-directory-resources g/Any
          (g/fnk [_node-id resource-map settings-map]
            (let [custom-resources-setting (get settings-map ["project" "custom_resources"])
                  directory-proj-paths (parse-custom-resource-paths custom-resources-setting)

                  directory-resources
                  (coll/into-> directory-proj-paths []
                    (map (fn [directory-proj-path]
                           (or (get resource-map directory-proj-path)
                               (g/map->error
                                 {:_node-id _node-id
                                  :severity :fatal
                                  :message (format "Custom resources directory not found: '%s'" directory-proj-path)})))))]

              (g/precluding-errors directory-resources
                directory-resources))))

  (output custom-resource+versions g/Any :cached
          (g/fnk [custom-resources-directory-resources resource-snapshot ssl-certificates-directory-resource]
            ;; We depend on the resource-snapshot to ensure this output reflects
            ;; the on-disk state of all the involved resources.
            (let [status-map (:status-map resource-snapshot)

                  directory-resources
                  (cond-> custom-resources-directory-resources
                          ssl-certificates-directory-resource (conj ssl-certificates-directory-resource))

                  custom-resources
                  (coll/into-> directory-resources []
                    (map resource/resource-seq)
                    coll/flatten-xf
                    (distinct)
                    (filter file-resource?))]

              ;; We include the version only to ensure this output is
              ;; invalidated if any of the included files change on disk.
              (coll/into-> custom-resources []
                (map (fn [resource]
                       (let [proj-path (resource/proj-path resource)
                             resource-status (status-map proj-path)
                             version (:version resource-status)]
                         (pair resource version))))))))

  (output custom-build-targets g/Any :cached
          (g/fnk [_node-id custom-resource+versions]
            (try
              (mapv (fn [[source-resource _version]]
                      ;; We don't actually need the version here, since we will
                      ;; generate a hash from the contents of each file.
                      (pipeline/make-source-bytes-build-target _node-id (->CustomResource source-resource)))
                    custom-resource+versions)
              (catch Throwable error
                (g/map->error
                  {:_node-id _node-id
                   :_label :custom-build-targets
                   :message (ex-message error)
                   :severity :fatal})))))

  (input save-value g/Any)
  (output save-value g/Any (gu/passthrough save-value))

  (output build-targets g/Any :cached produce-build-targets))

;;; loading node

(defn- load-game-project [project self resource source-value]
  (let [graph-id (g/node-id->graph-id self)
        workspace (resource/workspace resource)
        resource-setting-connections (reduce-kv (fn [m k v] (assoc m k [self v])) {} resource-setting-connections-template)]
    (concat
      (g/connect workspace :resource-map self :resource-map)
      (g/connect workspace :resource-snapshot self :resource-snapshot)
      (g/connect workspace :dependencies self :dependencies)
      (g/make-nodes graph-id [settings-node settings/SettingsNode]
        (g/connect settings-node :_node-id self :nodes)
        (g/connect settings-node :settings-map self :settings-map)
        (g/connect settings-node :save-value self :save-value)
        (g/connect settings-node :form-data self :form-data)
        (g/connect settings-node :meta-info self :meta-info)
        (g/connect settings-node :resource-settings self :resource-settings)
        (g/connect settings-node :setting-errors self :build-errors)
        (settings/load-settings-node project self settings-node resource source-value gpcore/basic-meta-info resource-setting-connections)))))

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
