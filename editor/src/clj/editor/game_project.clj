(ns editor.game-project
  (:require [clojure.java.io :as io]
            [clojure.string :as s]
            [dynamo.graph :as g]
            [editor.project :as project]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$PrototypeDesc  GameObject$ComponentDesc GameObject$EmbeddedComponentDesc GameObject$PrototypeDesc$Builder]
           [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.dynamo.proto DdfMath$Point3 DdfMath$Quat]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader StringReader BufferedReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d]))

(def game-project-icon "icons/32/Icons_04-Project-file.png")

;;; parsing raw .properties/.project file to string maps & order info

(defn- non-blank [vals]
  (remove s/blank? vals))

(defn- trimmed [lines]
  (map s/trim lines))

(defn- read-setting-lines [setting-reader]
  (non-blank (trimmed (line-seq setting-reader))))

(defn- resource-reader [resource-name]
  (io/reader (io/resource resource-name)))

(defn- string-reader [content]
  (BufferedReader. (StringReader. content)))

(defn- empty-parse-state []
  {:current-category nil
   :settings {}
   :category-order nil
   :setting-order {}})

(defn- parse-category-line [{:keys [settings category-order] :as parse-state} line]
  (when (.startsWith line "[")
    (let [new-category (subs line 1 (dec (count line)))
          new-category-order (conj category-order new-category)]
      (assoc parse-state
             :current-category new-category
             :category-order new-category-order))))

(defn- parse-setting-line [{:keys [current-category settings setting-order] :as parse-state} line]
  (let [sep (.indexOf line "=")]
    (when (not= sep -1)
      (let [setting-path (non-blank (s/split (s/trim (subs line 0 sep)) #"\."))
            val (s/trim (subs line (inc sep)))]
        (when-let [setting-key (first setting-path)]
          (let [new-settings (assoc-in settings (cons current-category setting-path) val)
                new-setting-order (if (some #{setting-key} (setting-order current-category))
                                    setting-order
                                    (update-in setting-order [current-category] conj setting-key))]
            (assoc parse-state
                   :settings new-settings
                   :setting-order new-setting-order)))))))


(defn- parse-state->info [{:keys [settings category-order setting-order]}]
  {:settings settings
   :category-order (distinct (reverse category-order))
   :setting-order (into {} (for [[category settings] setting-order] [category (reverse settings)]))})

(defn- parse-settings [reader]
  (parse-state->info (reduce
                      (fn [parse-state line]
                        (or (parse-category-line parse-state line)
                            (parse-setting-line parse-state line)
                            parse-state))
                      (empty-parse-state)
                      (read-setting-lines reader))))


(defn- order-category-settings [settings setting-order]
  (remove nil?
          (map (fn [setting]
                 (when (contains? settings setting)
                   {:setting setting :value (settings setting)}))
               setting-order)))

(defn- order-settings [settings category-order setting-order]
  (remove nil?
          (map (fn [category]
                 (when-let [settings (seq (order-category-settings (get settings category) (get setting-order category)))]
                   {:category category :settings settings}))
               category-order)))

;;; translating/sanitizing type field, discarding ill-formed meta settings

(def ^:private type-map
  {"resource" :resource
   "string" String
   "bool" g/Bool
   "integer" g/Int
   "number" g/Num})

(defmulti parse-setting-value (fn [type _] type))

(defmethod parse-setting-value String [_ raw]
  raw)

(defmethod parse-setting-value g/Bool [_ raw]
  (try
    (Boolean/parseBoolean raw)
    (catch Throwable _
      (boolean (Integer/parseInt raw)))))

(defmethod parse-setting-value g/Int [_ raw]
  (Integer/parseInt raw))

(defmethod parse-setting-value g/Num [_ raw]
  (Double/parseDouble raw))

(defmethod parse-setting-value :resource [_ raw]
  raw)

(defn- parse-setting-value-or-nil [type raw]
  (try
    (parse-setting-value type raw)
    (catch Throwable _
      nil)))

(defn- sanitize-meta-setting [[key value]]
  (if (= key "help")
    (when (string? value)
      [key value])
    (when (map? value)
      (let [{:strs [type filter help default]} value]
        (when-let [type (type-map type)]
          (let [default (and default (parse-setting-value-or-nil type default))]
            [key {:type type :filter filter :help help :default default}]))))))

(defn- sanitize-meta-settings [raw-meta-settings]
  (into {} (for [[category settings] raw-meta-settings]
             [category (into {} (remove nil? (map sanitize-meta-setting settings)))])))

(defn- sanitize-setting-value [meta-settings category [key value]]
  (when-let [type (get-in meta-settings [category key :type])]
    (let [parsed-value (parse-setting-value-or-nil type value)]
      (when (not (nil? parsed-value))
        [key parsed-value]))))

(defn- sanitize-settings [meta-settings raw-settings]
  (into {} (for [[category settings] raw-settings]
             [category (into {} (remove nil? (map (partial sanitize-setting-value meta-settings category) settings)))])))
  
;;; transforming to form data

(defn- make-form-field [{:keys [setting value]}]
  (let [{:keys [type help default filter]} value]
    (when (not= setting "help")
      {:field setting
       :help help
       :type type
       :filter filter
       :default default})))

(defn- make-form-section [category]
  (let [category-name (:category category)
        all-settings (:settings category)
        fields (remove nil? (map make-form-field all-settings))
        [help-setting] (filter #(= (:setting %) "help") all-settings)
        category-help (:value help-setting)]
    {:section category-name
     :help category-help
     :title category-name
     :fields fields}))

(defn- make-form-sections [meta-info]
  (let [ordered (order-settings (:settings meta-info) (:category-order meta-info) (:setting-order meta-info))]
    (map make-form-section ordered)))

(defn- make-form-data [form-ctxt form-sections form-values]
  {:ctxt form-ctxt
   :sections form-sections
   :values form-values})

;;; meta settings

(defonce ^:private standard-meta-info
  (let [parsed (parse-settings (resource-reader "meta.properties"))]
    (update-in parsed [:settings] sanitize-meta-settings)))

(defn- flat-settings [settings]
  (remove (comp map? second)
          (tree-seq
           (comp map? second)
           (fn [node]
             (map (fn [child] [(conj (first node) (first child)) (second child)])
                  (seq (second node))))
           [[] settings])))

(defn- extra-setting-meta-info []
  {:type String
   :help "unknown setting"
   :default nil
   :filter nil})

(defn- create-extra-meta-info [standard-meta-info {:keys [settings category-order setting-order]}]
  (let [meta-settings (reduce
                       (fn [m [path _]]
                         (assoc-in m path (extra-setting-meta-info)))
                       {}
                       (flat-settings settings))]
    {:settings meta-settings
     :category-order category-order
     :setting-order setting-order}))

(defn- merge-settings [settings default]
  (merge-with merge default settings))

(defn- distinct-concat [order default]
  (distinct (concat order default)))

(defn- merge-category-order [order default]
  (distinct-concat order default))

(defn- merge-setting-order [order default]
  (merge-with distinct-concat order default))

(defn- merge-meta-info [original default]
  {:settings (merge-settings (:settings original) (:settings default))
   :category-order (merge-category-order (:category-order original) (:category-order default))
   :setting-order (merge-setting-order (:setting-order original) (:setting-order default))})

;;; accessing (meta-)settings

(defn game-settings [resource-node]
  (:settings (g/node-value resource-node :setting-info)))

(defn game-meta-settings [resource-node]
  (:settings (g/node-value resource-node :meta-info)))

(defn- setting [settings meta-settings category key]
  (get-in settings [category key] (get-in meta-settings [category key :default])))

(defn get-setting [resource-node category key]
  (setting (game-settings resource-node) (game-meta-settings resource-node) category key))

;;; loading node

(defn- root-resource [base-resource settings meta-settings category key]
  (let [path (setting settings meta-settings category key)
        ; TODO - hack for compiled files in game.project
        path (subs path 0 (dec (count path)))
        ; TODO - hack for inconsistencies in game.project paths
        path (if (.startsWith path "/")
               path
               (str "/" path))]
    (workspace/resolve-resource base-resource path)))

(defn- load-game-project [project self input]
  (let [raw-setting-info (parse-settings (string-reader (slurp input)))
        effective-meta-info (merge-meta-info standard-meta-info (create-extra-meta-info standard-meta-info raw-setting-info))
        sanitized-settings (sanitize-settings (:settings effective-meta-info) (:settings raw-setting-info))
        setting-info (assoc raw-setting-info :settings sanitized-settings)
        resource   (g/node-value self :resource)
        roots      (map (fn [[category field]] (root-resource resource sanitized-settings (:settings effective-meta-info) category field))
                        [["bootstrap" "main_collection"] ["input" "game_binding"] ["input" "gamepads"]
                         ["bootstrap" "render"] ["display" "display_profiles"]])]
    (concat
     (g/set-property self :setting-info setting-info :meta-info effective-meta-info)
     (for [root roots]
       (project/connect-resource-node project root self [[:build-targets :dep-build-targets]])))))

(defn- ordered-setting->string [{:keys [setting value]}]
  (str setting " = " value))

(defn- ordered-category->string [ordered-category]
  (str "[" (:category ordered-category) "]"
       "\n"
       (s/join "\n" (map ordered-setting->string (:settings ordered-category)))))

(defn- ordered-settings->string [ordered-settings]
  (s/join "\n\n" (map ordered-category->string ordered-settings)))

(defn- setting-info->string [setting-info]
  (let [extra-meta-info (create-extra-meta-info standard-meta-info setting-info)
        category-order (merge-category-order (:category-order extra-meta-info) (:category-order standard-meta-info))
        setting-order (merge-setting-order (:setting-order extra-meta-info) (:setting-order standard-meta-info))]
    (ordered-settings->string (order-settings (:settings setting-info) category-order setting-order))))
  
(g/defnk produce-save-data [resource setting-info]
  {:resource resource :content (setting-info->string setting-info)})

(defn- build-game-project [self basis resource dep-resources user-data]
  {:resource resource :content (.getBytes (:content user-data))})

(g/defnk produce-form-data [_node-id meta-info setting-info]
  (make-form-data [_node-id [:setting-info :settings]] (make-form-sections meta-info) (:settings setting-info)))

(g/defnode GameProjectNode
  (inherits project/ResourceNode)

  (property setting-info g/Any (dynamic visible (g/always false)))
  (property meta-info g/Any (dynamic visible (g/always false)))
  
  (output form-data g/Any :cached produce-form-data)

  (input dep-build-targets g/Any :array)

  (output outline g/Any :cached (g/fnk [_node-id] {:node-id _node-id :label "Game Project" :icon game-project-icon}))
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached (g/fnk [_node-id resource setting-info dep-build-targets]
                                             [{:node-id _node-id
                                               :resource (workspace/make-build-resource resource)
                                               :build-fn build-game-project
                                               :user-data {:content (setting-info->string setting-info)}
                                               :deps (vec (flatten dep-build-targets))}])))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "project"
                                    :label "Project"
                                    :node-type GameProjectNode
                                    :load-fn load-game-project
                                    :icon game-project-icon
                                    :view-types [:form-view :text]))
