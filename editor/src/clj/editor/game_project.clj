(ns editor.game-project
  (:require [clojure.string :as string]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [util.murmur :as murmur]
            [editor.graph-util :as gu]
            [editor.game-project-core :as gpcore]
            [editor.defold-project :as project]
            [camel-snake-kebab :as camel]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [service.log :as log])
  (:import [java.io File]
           [org.apache.commons.io IOUtils]))

(set! *warn-on-reflection* true)

(def game-project-icon "icons/32/Icons_04-Project-file.png")

(defn- label [key]
  (-> key
      name
      camel/->Camel_Snake_Case_String
      (string/replace "_" " ")))

(defn- make-form-field [setting]
  (assoc setting
         :label (label (second (:path setting)))
         :optional true))

(defn- make-form-section [category-name category-info settings]
  {:title category-name
   :help (:help category-info)
   :fields (map make-form-field settings)})

(defn- make-form-values-map [settings]
  (into {} (map (juxt :path :value) settings)))

(defn- make-form-data [form-ops meta-info settings]
  (let [meta-settings (:settings meta-info)
        meta-category-map (:categories meta-info)
        categories (distinct (map gpcore/setting-category meta-settings))
        category-settings (group-by gpcore/setting-category meta-settings)
        sections (map #(make-form-section % (get-in meta-info [:categories %]) (category-settings %)) categories)
        values (make-form-values-map settings)]
    {:form-ops form-ops :sections sections :values values}))

;;; loading node

(defn- root-resource [base-resource settings meta-settings category key]
  (let [path (gpcore/get-setting-or-default meta-settings settings [category key])]
    (workspace/resolve-resource base-resource path)))

(def ^:private path->property {["display" "display_profiles"] :display-profiles
                               ["bootstrap" "main_collection"] :main-collection
                               ["bootstrap" "render"] :render
                               ["graphics" "texture_profiles"] :texture-profiles
                               ["input" "gamepads"] :gamepads
                               ["input" "game_binding"] :input-binding})
(def ^:private property->path (clojure.set/map-invert path->property))

(defn- load-game-project [project self input]
  (let [raw-settings (gpcore/parse-settings (gpcore/string-reader (slurp input)))
        meta-info (gpcore/complement-meta-info gpcore/basic-meta-info raw-settings)
        meta-settings (:settings meta-info)
        sanitized-settings (gpcore/sanitize-settings meta-settings raw-settings) ; this provokes parse errors if any
        resource   (g/node-value self :resource)
        roots      (into {} (map (fn [[category field]] [[category field] (root-resource resource sanitized-settings meta-settings category field)])
                                 (keys path->property)))]
    (concat
      ;; We retain the actual raw string settings and update these when/if the user changes a setting,
      ;; rather than parse to proper typed/sanitized values and rendering these on save - again only to
      ;; reduce diffs.
     (g/set-property self :raw-settings raw-settings :meta-info meta-info)
     (g/connect project :resource-map self :resource-map)
      (for [[p root] roots]
        (g/set-property self (path->property p) root)))))

(g/defnk produce-save-data [resource raw-settings meta-info _declared-properties]
  (let [content (->> raw-settings
                  gpcore/settings-with-value
                  (map (fn [s] (if (contains? path->property (:path s))
                                 (update s :value #(str % "c"))
                                 s)))
                  gpcore/settings->str)]
    {:resource resource
     :content content}))

(defn- build-game-project [self basis resource dep-resources user-data]
  (let [ref-deps (:ref-deps user-data)
        settings (map (fn [s]
                        (if (contains? path->property (:path s))
                          (let [ref (ref-deps (path->property (:path s)))]
                            (assoc s :value (resource/proj-path (dep-resources ref))))
                          s))
                      (gpcore/settings-with-value (:settings user-data)))
        ^String user-data-content (gpcore/settings->str settings)]
    {:resource resource :content (.getBytes user-data-content)}))

(defn- resource-content [resource]
  (IOUtils/toByteArray (io/input-stream resource)))

(defn- build-custom-resource [self basis resource dep-resources user-data]
  {:resource resource :content (resource-content (:resource resource))})

(defrecord CustomResource [resource]
  ;; Only purpose is to provide resource-type with :build-ext = :ext
  resource/Resource
  (children [this] (resource/children resource))
  (ext [this] (resource/ext resource))
  (resource-type [this]
    (let [ext (resource/ext this)]
      {:ext ext
       :label "Custom Resource"
       :build-ext ext}))
  (source-type [this] (resource/source-type resource))
  (exists? [this] (resource/exists? resource))
  (read-only? [this] (resource/read-only? resource))
  (path [this] (resource/path resource))
  (abs-path [this] (resource/abs-path resource))
  (proj-path [this] (resource/proj-path resource))
  (url [this] (resource/url resource))
  (resource-name [this] (resource/resource-name resource))
  (workspace [this] (resource/workspace resource))
  (resource-hash [this] (resource/resource-hash resource))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/input-stream resource))
  (io/make-reader        [this opts] (io/reader resource))
  (io/make-output-stream [this opts] (io/output-stream resource))
  (io/make-writer        [this opts] (io/writer resource)))

(defn- make-custom-build-target [node-id resource]
  {:node-id node-id
   :resource (workspace/make-build-resource (CustomResource. resource))
   :build-fn build-custom-resource
   ;; NOTE! Break build cache when resource content changes.
   :user-data {:hash (murmur/hash64-bytes (resource-content resource))}})

(defn- with-leading-slash [path]
  (if (string/starts-with? path "/")
    path
    (str "/" path)))

(defn- strip-trailing-slash [path]
  (string/replace path #"/*$" ""))

(defn- file-resource? [resource]
  (= (resource/source-type resource) :file))

(defn- find-custom-resources [resource-map custom-paths]
  (->> (flatten (keep (fn [custom-path]
                      (when-let [base-resource (resource-map custom-path)]
                        (resource/resource-seq base-resource)))
                    custom-paths))
       (distinct)
       (filter file-resource?)))

(defn- parse-custom-resource-paths [cr-setting]
  (let [paths (remove string/blank? (map string/trim (string/split (or cr-setting "")  #",")))]
    (map (comp strip-trailing-slash with-leading-slash) paths)))

(defn- set-setting [settings {:keys [path] :as meta-setting} value]
  (gpcore/set-setting settings meta-setting path value))

(defn- set-form-op [{:keys [node-id meta-settings]} path value]
  (if (contains? path->property path)
    (g/set-property! node-id (path->property path) value)
    (let [meta-setting (nth meta-settings (gpcore/setting-index meta-settings path))
          value (if (satisfies? resource/Resource value)
                  (resource/proj-path value)
                  value)]
      (g/update-property! node-id :raw-settings set-setting meta-setting value))))

(defn- clear-form-op [{:keys [node-id meta-settings]} path]
  (if (contains? path->property path)
    (let [default-resource (workspace/resolve-resource (g/node-value node-id :resource)
                                                       (gpcore/get-default-setting meta-settings path))]
      (g/set-property! node-id (path->property path) default-resource))
    (g/update-property! node-id :raw-settings gpcore/clear-setting path)))

(defn- make-form-ops [node-id meta-settings]
  {:user-data {:node-id node-id :meta-settings meta-settings}
   :set set-form-op
   :clear clear-form-op})

(g/defnk produce-settings-map [meta-info raw-settings]
  (let [meta-settings (:settings meta-info)
        default-settings (gpcore/make-default-settings meta-settings)
        sanitized-settings (gpcore/sanitize-settings meta-settings (gpcore/settings-with-value raw-settings))
        all-settings (concat default-settings sanitized-settings)]
    (gpcore/make-settings-map all-settings)))

(g/defnk produce-form-data [_node-id meta-info raw-settings]
  (let [meta-settings (:settings meta-info)
        sanitized-settings (gpcore/sanitize-settings meta-settings (gpcore/settings-with-value raw-settings))]
    (make-form-data (make-form-ops _node-id meta-settings) meta-info sanitized-settings)))

(g/defnode GameProjectRefs
  (property display-profiles resource/Resource
          (dynamic visible (g/constantly false))
          (value (gu/passthrough display-profiles-resource))
          (set (fn [basis self old-value new-value]
                 (project/resource-setter basis self old-value new-value
                                              [:resource :display-profiles-resource]
                                              [:build-targets :dep-build-targets]
                                              [:profile-data :display-profiles-data]))))
  (property main-collection resource/Resource
            (dynamic visible (g/constantly false))
            (value (gu/passthrough main-collection-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                                [:resource :main-collection-resource]
                                                [:build-targets :dep-build-targets]))))
  (property render resource/Resource
            (dynamic visible (g/constantly false))
            (value (gu/passthrough render-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                                [:resource :render-resource]
                                                [:build-targets :dep-build-targets]))))
  (property texture-profiles resource/Resource
            (dynamic visible (g/constantly false))
            (value (gu/passthrough texture-profiles-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                                [:resource :texture-profiles-resource]
                                                [:build-targets :dep-build-targets]))))
  (property gamepads resource/Resource
            (dynamic visible (g/constantly false))
            (value (gu/passthrough gamepads-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                                [:resource :gamepads-resource]
                                                [:build-targets :dep-build-targets]))))
  (property input-binding resource/Resource
            (dynamic visible (g/constantly false))
            (value (gu/passthrough input-binding-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                                [:resource :input-binding-resource]
                                                [:build-targets :dep-build-targets]))))

  (input display-profiles-data g/Any)
  (input display-profiles-resource resource/Resource)
  (input main-collection-resource resource/Resource)
  (input render-resource resource/Resource)
  (input texture-profiles-resource resource/Resource)
  (input gamepads-resource resource/Resource)
  (input input-binding-resource resource/Resource)

  (output display-profiles-data g/Any (gu/passthrough display-profiles-data))

  (output ref-settings g/Any (g/fnk [_declared-properties]
                                    (let [p (-> (:properties _declared-properties)
                                              (select-keys (keys property->path)))]
                                      (into {} (map (fn [[k v]] [(property->path k) (str (some-> (:value v) resource/proj-path))]) p))))))

(g/defnode GameProjectNode
  (inherits project/ResourceNode)
  (inherits GameProjectRefs)

  (property raw-settings g/Any (dynamic visible (g/constantly false)))
  (property meta-info g/Any (dynamic visible (g/constantly false)))

  (output raw-settings g/Any
          (g/fnk [raw-settings meta-info ref-settings]
                 (reduce (fn [raw [p v]]
                           (let [meta (:settings meta-info)
                                 old (gpcore/get-setting-or-default meta raw-settings p)]
                             (if (not= old v)
                               (gpcore/set-setting raw meta p v)
                               raw)))
                         raw-settings ref-settings)))
  (output settings-map g/Any :cached produce-settings-map)
  (output form-data g/Any :cached produce-form-data)

  (input resource-map g/Any)
  (input dep-build-targets g/Any :array)

  (output custom-build-targets g/Any :cached (g/fnk [_node-id resource-map settings-map]
                                                    (let [custom-paths (parse-custom-resource-paths (get settings-map ["project" "custom_resources"]))]
                                                      (map (partial make-custom-build-target _node-id)
                                                           (find-custom-resources resource-map custom-paths)))))

  (output outline g/Any :cached (g/fnk [_node-id] {:node-id _node-id :label "Game Project" :icon game-project-icon}))
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached (g/fnk [_node-id resource raw-settings custom-build-targets dep-build-targets _declared-properties]
                                             (let [ref-deps (into {} (map (fn [[k v]] [k (workspace/make-build-resource (:value v))])
                                                                          (select-keys (:properties _declared-properties) (keys property->path))))]
                                               [{:node-id _node-id
                                                 :resource (workspace/make-build-resource resource)
                                                 :build-fn build-game-project
                                                 :user-data {:settings raw-settings
                                                             :ref-deps ref-deps}
                                                 :deps (vec (concat (flatten dep-build-targets)
                                                                    custom-build-targets))}]))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :textual? true
                                    :ext "project"
                                    :label "Project"
                                    :node-type GameProjectNode
                                    :load-fn load-game-project
                                    :icon game-project-icon
                                    :view-types [:form-view :text]))
