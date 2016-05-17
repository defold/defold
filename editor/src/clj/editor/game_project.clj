(ns editor.game-project
  (:require [clojure.string :as s]
            [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [editor.game-project-core :as gpcore]
            [editor.defold-project :as project]
            [camel-snake-kebab :as camel]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [service.log :as log]))

(set! *warn-on-reflection* true)

(def game-project-icon "icons/32/Icons_04-Project-file.png")

(defn- label [key]
  (-> key
      name
      camel/->Camel_Snake_Case_String
      (s/replace "_" " ")))

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

(def ^:private settings-map-substitute (gpcore/make-settings-map (gpcore/make-default-settings (:settings gpcore/basic-meta-info))))

(g/defnode GameProjectSettingsProxy
  (input settings-map g/Any :substitute settings-map-substitute)
  (output settings-map g/Any :cached (g/fnk [settings-map] settings-map)))

(def ^:private path->property {["display" "display_profiles"] :display-profiles
                               ["bootstrap" "main_collection"] :main-collection
                               ["bootstrap" "render"] :render
                               ["graphics" "texture_profiles"] :texture-profiles
                               ["input" "gamepads"] :gamepads
                               ["input" "game_binding"] :input-binding})
(def ^:private property->path (clojure.set/map-invert path->property))

(defn- load-game-project [project self input]
  (g/make-nodes
   (g/node-id->graph-id self)
   [proxy [GameProjectSettingsProxy]]
   (g/connect self :display-profiles-data project :display-profiles)
   (g/connect proxy :settings-map project :settings)
   (g/connect proxy :_node-id self :nodes)
   (try
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
        (for [[p root] roots]
          (g/set-property self (path->property p) root))))
     (catch java.lang.Exception e
       (log/warn :exception e)
       (g/mark-defective self (g/error-severe {:type :invalid-content :message (.getMessage e)}))))
   (g/connect self :settings-map proxy :settings-map)))

(g/defnk produce-save-data [resource raw-settings meta-info _declared-properties]
  (let [content (->> raw-settings
                  gpcore/settings-with-value
                  (map (fn [s] (if (contains? path->property (:path s))
                                 (update s :value #(str % "c"))
                                 s)))
                  gpcore/settings->str)]
    {:resource resource :content content}))

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
  (property display-profiles resource/ResourceType
          (dynamic visible false)
          (value (g/fnk [display-profiles-resource] display-profiles-resource))
          (set (project/gen-resource-setter [[:resource :display-profiles-resource]
                                             [:build-targets :dep-build-targets]
                                             [:profile-data :display-profiles-data]])))
  (property main-collection resource/ResourceType
            (dynamic visible false)
            (value (g/fnk [main-collection-resource] main-collection-resource))
            (set (project/gen-resource-setter [[:resource :main-collection-resource]
                                               [:build-targets :dep-build-targets]])))
  (property render resource/ResourceType
            (dynamic visible false)
            (value (g/fnk [render-resource] render-resource))
            (set (project/gen-resource-setter [[:resource :render-resource]
                                               [:build-targets :dep-build-targets]])))
  (property texture-profiles resource/ResourceType
            (dynamic visible false)
            (value (g/fnk [texture-profiles-resource] texture-profiles-resource))
            (set (project/gen-resource-setter [[:resource :texture-profiles-resource]
                                               [:build-targets :dep-build-targets]])))
  (property gamepads resource/ResourceType
            (dynamic visible false)
            (value (g/fnk [gamepads-resource] gamepads-resource))
            (set (project/gen-resource-setter [[:resource :gamepads-resource]
                                               [:build-targets :dep-build-targets]])))
  (property input-binding resource/ResourceType
            (dynamic visible false)
            (value (g/fnk [input-binding-resource] input-binding-resource))
            (set (project/gen-resource-setter [[:resource :input-binding-resource]
                                               [:build-targets :dep-build-targets]])))

  (input display-profiles-data g/Any)
  (input display-profiles-resource resource/ResourceType)
  (input main-collection-resource resource/ResourceType)
  (input render-resource resource/ResourceType)
  (input texture-profiles-resource resource/ResourceType)
  (input gamepads-resource resource/ResourceType)
  (input input-binding-resource resource/ResourceType)

  (output display-profiles-data g/Any (g/fnk [display-profiles-data] display-profiles-data))

  (output ref-settings g/Any (g/fnk [_declared-properties]
                                    (let [p (-> (:properties _declared-properties)
                                              (select-keys (keys property->path)))]
                                      (into {} (map (fn [[k v]] [(property->path k) (str (some-> (:value v) resource/proj-path))]) p))))))

(g/defnode GameProjectNode
  (inherits project/ResourceNode)
  (inherits GameProjectRefs)

  (property raw-settings g/Any (dynamic visible false))
  (property meta-info g/Any (dynamic visible false))

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

  (input dep-build-targets g/Any :array)

  (output outline g/Any :cached (g/fnk [_node-id] {:node-id _node-id :label "Game Project" :icon game-project-icon}))
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached (g/fnk [_node-id resource raw-settings meta-info dep-build-targets _declared-properties]
                                             (let [ref-deps (into {} (map (fn [[k v]] [k (workspace/make-build-resource (:value v))])
                                                                          (select-keys (:properties _declared-properties) (keys property->path))))]
                                               [{:node-id _node-id
                                                :resource (workspace/make-build-resource resource)
                                                :build-fn build-game-project
                                                :user-data {:settings raw-settings
                                                            :ref-deps ref-deps}
                                                :deps (vec (flatten dep-build-targets))}]))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "project"
                                    :label "Project"
                                    :node-type GameProjectNode
                                    :load-fn load-game-project
                                    :icon game-project-icon
                                    :view-types [:form-view :text]))
