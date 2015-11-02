(ns editor.game-project
  (:require [clojure.java.io :as io]
            [clojure.string :as s]
            [clojure.edn :as edn]
            [dynamo.graph :as g]
            [editor.project :as project]
            [camel-snake-kebab :as camel]
            [editor.workspace :as workspace]
            [service.log :as log])
  (:import [java.io PushbackReader StringReader BufferedReader]))

(def game-project-icon "icons/32/Icons_04-Project-file.png")

(defn- non-blank [vals]
  (remove s/blank? vals))

(defn- trimmed [lines]
  (map s/trim lines))

(defn- read-setting-lines [setting-reader]
  (non-blank (trimmed (line-seq setting-reader))))

(defn- resource-reader [resource-name]
  (io/reader (io/resource resource-name)))

(defn- pushback-reader [reader]
  (PushbackReader. reader))

(defn- string-reader [content]
  (BufferedReader. (StringReader. content)))

(defn- empty-parse-state []
  {:current-category nil :settings nil})

(defn- parse-category-line [{:keys [current-category settings] :as parse-state} line]
  (when-let [[_ new-category] (re-find #"\[([^\]]*)\]" line)]
    (assoc parse-state :current-category new-category)))

(defn- parse-setting-line [{:keys [current-category settings] :as parse-state} line]
  (when-let [[_ key val] (seq (map s/trim (re-find #"([^=]+)=(.*)" line)))]
    (when-let [setting-path (seq (non-blank (s/split key #"\.")))]
      (update parse-state :settings conj {:path (cons current-category setting-path) :value val}))))

(defn- parse-error [line]
  (throw (Exception. (format "Invalid game.project line: %s" line))))

(defn- parse-state->settings [{:keys [settings]}]
  (vec (reverse settings)))

(defn- parse-settings [reader]
  (parse-state->settings (reduce
                          (fn [parse-state line]
                            (or (parse-category-line parse-state line)
                                (parse-setting-line parse-state line)
                                (parse-error line)))
                          (empty-parse-state)
                          (read-setting-lines reader))))

(defmulti parse-setting-value (fn [type ^String raw] type))

(defmethod parse-setting-value :string [_ raw]
  raw)

(defmethod parse-setting-value :boolean [_ raw]
  ;; this is roughly how the old editor does it, rather than != 0.
  (= raw "1"))

(defmethod parse-setting-value :integer [_ raw]
  (Integer/parseInt raw))

(defmethod parse-setting-value :number [_ raw]
  (Double/parseDouble raw))

(defmethod parse-setting-value :resource [_ raw]
  raw)

(def ^:private type-defaults
  {:string ""
   :boolean false
   :integer 0
   :number 0.0
   :resource ""})

(defn- add-type-defaults [meta-info]
  (update-in meta-info [:settings]
             (partial map (fn [setting] (update setting :default #(if (nil? %) (type-defaults (:type setting)) %))))))

(def ^:private basic-meta-info (add-type-defaults (edn/read (pushback-reader (resource-reader "meta.edn")))))

(defn- make-meta-settings-for-unknown [meta-settings settings]
  (let [known-settings (set (map :path meta-settings))
        unknown-settings (remove known-settings (map :path settings))]
    (map (fn [setting-path] {:path setting-path :type :string :help "unknown setting"}) unknown-settings)))

(defn- complement-meta-info [meta-info settings]
  (update meta-info :settings
          #(concat % (make-meta-settings-for-unknown % settings))))

(defn- make-meta-settings-map [meta-settings]
  (zipmap (map :path meta-settings) meta-settings))

(defn- trim-trailing-c [value]
  (if (= (last value) \c)
    (subs value 0 (dec (count value)))
    value))

(defn- sanitize-value [{:keys [type preserve-extension]} value]
  (if (and (= type :resource) (not preserve-extension))
    (trim-trailing-c value)
    value))

(defn- sanitize-setting [meta-settings-map {:keys [path] :as setting}]
  (when-let [{:keys [type] :as meta-setting} (meta-settings-map path)]
    (update setting :value
            #(do (->> %
                     (parse-setting-value type)
                     (sanitize-value meta-setting))))))

(defn- sanitize-settings [meta-settings settings]
  (vec (map (partial sanitize-setting (make-meta-settings-map meta-settings)) settings)))

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

(defn- make-default-settings [meta-settings]
  (map (fn [meta-setting]
         {:path (:path meta-setting) :value (:default meta-setting)})
        meta-settings))

(defn- make-form-values-map [settings]
  (into {} (map (juxt :path :value) settings)))

(def ^:private setting-category (comp first :path))

(defn- make-form-data [form-ops meta-info settings]
  (let [meta-settings (:settings meta-info)
        meta-category-map (:categories meta-info)
        categories (distinct (map setting-category meta-settings))
        category-settings (group-by setting-category meta-settings)
        sections (map #(make-form-section % (get-in meta-info [:categories %]) (category-settings %)) categories)
        values (make-form-values-map settings)]
    {:form-ops form-ops :sections sections :values values}))

(defn- category-order [settings]
  (distinct (map setting-category settings)))

(defn- category-grouped-settings [settings]
  (group-by setting-category settings))

(defn- setting->str [{:keys [path value]}]
  (let [key (s/join "." (rest path))]
    (str key " = " value)))

(defn- category->str [category settings]
  (s/join "\n" (cons (str "[" category "]") (map setting->str settings))))

(defn- settings->str [settings]
  (let [cat-order (category-order settings)
        cat-grouped-settings (category-grouped-settings settings)]
    ;; Here we interleave categories with \n\n rather than join to make sure the file also ends with
    ;; two consecutive newlines. This is purely to avoid whitespace diffs when loading a project
    ;; created in the old editor and saving.
    (s/join (interleave (map #(category->str % (cat-grouped-settings %)) cat-order) (repeat "\n\n")))))

(defn- setting-index [settings path]
  (first (keep-indexed (fn [index item] (when (= (:path item) path) index)) settings)))

(defn- get-setting [settings path]
  (when-let [index (setting-index settings path)]
    (:value (nth settings index))))

(defn- get-default-setting [meta-settings path]
  (when-let [index (setting-index meta-settings path)]
    (:default (nth meta-settings index))))

(defn- get-setting-or-default [meta-settings settings path]
  (or (get-setting settings path)
      (get-default-setting meta-settings path)))

;;; loading node

(defn- root-resource [base-resource settings meta-settings category key]
  (let [path (get-setting-or-default meta-settings settings [category key])]
    (workspace/resolve-resource base-resource path)))

(defn- make-settings-map [settings]
  (into {} (map (juxt :path :value) settings)))

(def ^:private settings-map-substitute (make-settings-map (make-default-settings (:settings basic-meta-info))))

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
     (let [raw-settings (parse-settings (string-reader (slurp input)))
           meta-info (complement-meta-info basic-meta-info raw-settings)
           meta-settings (:settings meta-info)
           sanitized-settings (sanitize-settings meta-settings raw-settings) ; this provokes parse errors if any
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

(defn- settings-with-value [settings]
  (filter #(contains? % :value) settings))

(g/defnk produce-save-data [resource raw-settings meta-info]
  {:resource resource :content (settings->str (settings-with-value raw-settings))})

(defn- build-game-project [self basis resource dep-resources user-data]
  (let [^String user-data-content (:content user-data)]
    {:resource resource :content (.getBytes user-data-content)}))

(defmulti render-raw-setting-value (fn [meta-setting value] (:type meta-setting)))

(defmethod render-raw-setting-value :boolean [_ value]
  (if value "1" "0"))

(defmethod render-raw-setting-value :resource [{:keys [preserve-extension]} value]
  (if (and (not (s/blank? value))
           (not preserve-extension))
    (str value "c")
    value))

(defmethod render-raw-setting-value :default [_ value]
  (str value))

(defn- set-setting [settings {:keys [path] :as meta-setting} value]
  (let [raw-value (render-raw-setting-value meta-setting value)]
    (if-let [index (setting-index settings path)]
      (assoc-in settings [index :value] raw-value)
      (conj settings {:path path :value raw-value}))))

(defn- set-form-op [{:keys [node-id meta-settings]} path value]
  (let [meta-setting (nth meta-settings (setting-index meta-settings path))]
    (g/update-property! node-id :raw-settings set-setting meta-setting value)))

(defn- clear-setting [settings path]
  (when-let [index (setting-index settings path)]
    (update settings index dissoc :value)))

(defn- clear-form-op [{:keys [node-id]} path]
  (g/update-property! node-id :raw-settings clear-setting path))

(defn- make-form-ops [node-id meta-settings]
  {:user-data {:node-id node-id :meta-settings meta-settings}
   :set set-form-op
   :clear clear-form-op})

(g/defnk produce-settings-map [meta-info raw-settings]
  (let [meta-settings (:settings meta-info)
        default-settings (make-default-settings meta-settings)
        sanitized-settings (sanitize-settings meta-settings (settings-with-value raw-settings))
        all-settings (concat default-settings sanitized-settings)]
    (make-settings-map all-settings)))

(g/defnk produce-form-data [_node-id meta-info raw-settings]
  (let [meta-settings (:settings meta-info)
        sanitized-settings (sanitize-settings meta-settings (settings-with-value raw-settings))]
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
                                               :user-data {:content (settings->str (settings-with-value raw-settings))}
                                               :deps (vec (flatten dep-build-targets))}])))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "project"
                                    :label "Project"
                                    :node-type GameProjectNode
                                    :load-fn load-game-project
                                    :icon game-project-icon
                                    :view-types [:form-view :text]))
