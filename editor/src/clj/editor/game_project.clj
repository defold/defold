(ns editor.game-project
  (:require [clojure.string :as s]
            [dynamo.graph :as g]
            [editor.game-project-core :as gpcore]
            [editor.project :as project]
            [camel-snake-kebab :as camel]
            [editor.workspace :as workspace]
            [service.log :as log]))

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

(defn- load-game-project [project self input]
  (g/make-nodes
   (g/node-id->graph-id self)
   [proxy [GameProjectSettingsProxy]]
   (g/connect proxy :settings-map project :settings)
   (g/connect proxy :_node-id self :nodes)
   (try
     (let [raw-settings (gpcore/parse-settings (gpcore/string-reader (slurp input)))
           meta-info (gpcore/complement-meta-info gpcore/basic-meta-info raw-settings)
           meta-settings (:settings meta-info)
           sanitized-settings (gpcore/sanitize-settings meta-settings raw-settings) ; this provokes parse errors if any
           resource   (g/node-value self :resource)
           roots      (map (fn [[category field]] (root-resource resource sanitized-settings meta-settings category field))
                           [["bootstrap" "main_collection"] ["input" "game_binding"] ["input" "gamepads"]
                            ["bootstrap" "render"] ["display" "display_profiles"]])]
       (concat
        ;; We retain the actual raw string settings and update these when/if the user changes a setting,
        ;; rather than parse to proper typed/sanitized values and rendering these on save - again only to
        ;; reduce diffs.
        (g/set-property self :raw-settings raw-settings :meta-info meta-info)
        (for [root roots]
          (project/connect-resource-node project root self [[:build-targets :dep-build-targets]]))))
     (catch java.lang.Exception e
       (log/warn :exception e)
       (g/mark-defective self (g/error-severe {:type :invalid-content :message (.getMessage e)}))))
   (g/connect self :settings-map proxy :settings-map)))

(g/defnk produce-save-data [resource raw-settings meta-info]
  {:resource resource :content (gpcore/settings->str (gpcore/settings-with-value raw-settings))})

(defn- build-game-project [self basis resource dep-resources user-data]
  (let [^String user-data-content (:content user-data)]
    {:resource resource :content (.getBytes user-data-content)}))

(defn- set-setting [settings {:keys [path] :as meta-setting} value]
  (gpcore/set-setting settings meta-setting path value))

(defn- set-form-op [{:keys [node-id meta-settings]} path value]
  (let [meta-setting (nth meta-settings (gpcore/setting-index meta-settings path))]
    (g/update-property! node-id :raw-settings set-setting meta-setting value)))

(defn- clear-form-op [{:keys [node-id]} path]
  (g/update-property! node-id :raw-settings gpcore/clear-setting path))

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

(g/defnode GameProjectNode
  (inherits project/ResourceNode)

  (property raw-settings g/Any (dynamic visible (g/always false)))
  (property meta-info g/Any (dynamic visible (g/always false)))

  (output settings-map g/Any :cached produce-settings-map)
  (output form-data g/Any :cached produce-form-data)

  (input dep-build-targets g/Any :array)

  (output outline g/Any :cached (g/fnk [_node-id] {:node-id _node-id :label "Game Project" :icon game-project-icon}))
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached (g/fnk [_node-id resource raw-settings meta-info dep-build-targets]
                                             [{:node-id _node-id
                                               :resource (workspace/make-build-resource resource)
                                               :build-fn build-game-project
                                               :user-data {:content (gpcore/settings->str (gpcore/settings-with-value raw-settings))}
                                               :deps (vec (flatten dep-build-targets))}])))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "project"
                                    :label "Project"
                                    :node-type GameProjectNode
                                    :load-fn load-game-project
                                    :icon game-project-icon
                                    :view-types [:form-view :text]))
