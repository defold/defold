;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.prefs
  "Editor preference system, using a global preference schema registry

  Schema is defined using maps with :type key that defines the meaning and
  additional properties of the schema. The schema is largely inspired by JSON
  schema, which is used in vscode extensions that contribute configuration
  used by the language servers. Supported types:
    :any           anything goes, no validation is performed
    :boolean       a boolean value
    :string        a string
    :keyword       a keyword
    :integer       an integer
    :number        floating point number
    :array         homogeneous typed array, requires :item key which defines
                   array item schema
    :set           typed set, requires :item key which defines set item schema
    :object        heterogeneous object, requires :properties key with a map of
                   keyword key to value schema
    :object-of     homogeneous object, requires :key and :val schema keys
    :enum          limited set of values, requires :values vector with available
                   values
    :tuple         heterogeneous fixed-length array, requires :items vector with
                   positional schemas
    :one-of        compositional schema, requires :schemas vector with at least
                   2 alternative schemas

  Additionally, each schema map supports these optional keys:
    :default        explicit default value to use instead of a type default
    :scope          either :global or :project
    :label          short description of the schema, a string
    :description    longer description of the schema, a string

  See also:
    https://code.visualstudio.com/api/references/contribution-points#contributes.configuration
    https://docs.google.com/document/d/17ke9huzMaagHAYmdzGGGRHnDD5ViLZrChT3OYaEXZuU/edit
    https://json-schema.org/understanding-json-schema/reference"
  (:refer-clojure :exclude [get])
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.spec.alpha :as s]
            [cognitect.transit :as transit]
            [editor.fs :as fs]
            [editor.os :as os]
            [util.coll :as coll]
            [util.eduction :as e]
            [util.fn :as fn])
  (:import [java.io ByteArrayInputStream PushbackReader]
           [java.nio.charset StandardCharsets]
           [java.nio.file Path]
           [java.util.concurrent Executors ScheduledExecutorService ThreadFactory TimeUnit]
           [java.util.prefs Preferences]))

(set! *warn-on-reflection* true)

;; All types, for reference:

#_[:any :boolean :string :keyword :integer :number :array :set :object :object-of :enum :tuple :one-of]

(defn- nilable [schema]
  {:type :one-of :schemas [{:type :enum :values [nil]} schema]})

(def default-schema
  {:type :object
   :properties
   {:asset-browser {:type :object
                    :properties
                    {:track-active-tab {:type :boolean :label "Track Active Tab in Asset Browser"}}}
    :input {:type :object
            :properties
            {:keymap-path {:type :string :label "Path to Custom Keymap"}}}
    :code {:type :object
           :properties
           {:custom-editor {:type :string}
            :open-file {:type :string :default "{file}"}
            :open-file-at-line {:type :string :default "{file}:{line}" :label "Open File at Line"}
            :font {:type :object
                   :properties
                   {:name {:type :string :default "Dejavu Sans Mono"}
                    :size {:type :number :default 12.0}}}
            :find {:type :object
                   :scope :project
                   :properties
                   {:term {:type :string}
                    :replacement {:type :string}
                    :whole-word {:type :boolean}
                    :case-sensitive {:type :boolean}
                    :wrap {:type :boolean :default true}}}
            :visibility {:type :object
                         :properties
                         {:indentation-guides {:type :boolean :default true}
                          :minimap {:type :boolean :default true}
                          :whitespace {:type :boolean :default true}}}}}
    :tools {:type :object
            :properties
            {:adb-path {:type :string
                        :label "ADB path"
                        :description "Path to ADB command that might be used to install and launch the Android app when it's bundled"}
             :ios-deploy-path {:type :string
                               :label "ios-deploy path"
                               :description "Path to ios-deploy command that might be used to install and launch iOS app when it's bundled"}}}
    :extensions {:type :object
                 :properties
                 {:build-server {:type :string}
                  :build-server-headers {:type :string}}}
    :search-in-files {:type :object
                      :scope :project
                      :properties
                      {:term {:type :string}
                       :exts {:type :string}
                       :include-libraries {:type :boolean :default true}}}
    :open-assets {:type :object
                  :scope :project
                  :properties
                  {:term {:type :string}}}
    :build {:type :object
            :scope :project
            :properties
            {:lint-code {:type :boolean :default true :label "Lint Code on Build"}
             :texture-compression {:type :boolean}}}
    :bundle {:type :object
             :scope :project
             :properties
             {:variant {:type :enum :values ["debug" "release" "headless"]}
              :texture-compression {:type :enum :values ["enabled" "disabled" "editor"]}
              :debug-symbols {:type :boolean :default true}
              :build-report {:type :boolean}
              :liveupdate {:type :boolean}
              :contentless {:type :boolean}
              :output-directory (nilable {:type :string})
              :open-output-directory {:type :boolean :default true}
              :android {:type :object
                        :properties
                        {:keystore (nilable {:type :string})
                         :keystore-pass (nilable {:type :string})
                         :key-pass (nilable {:type :string})
                         :architecture {:type :object
                                        :properties
                                        {:armv7-android {:type :boolean :default true}
                                         :arm64-android {:type :boolean}}}
                         :format (nilable {:type :enum :values ["apk" "aab" "apk,aab"]})
                         :install {:type :boolean}
                         :launch {:type :boolean}}}
              :macos {:type :object
                      :properties
                      {:architecture {:type :object
                                      :properties
                                      {:x86_64-macos {:type :boolean :default true}
                                       :arm64-macos {:type :boolean :default true}}}}}
              :ios {:type :object
                    :properties
                    {:sign {:type :boolean :default true}
                     :code-signing-identity (nilable {:type :string})
                     :provisioning-profile (nilable {:type :string})
                     :architecture {:type :object
                                    :properties
                                    {:arm64-ios {:type :boolean :default true}
                                     :x86_64-ios {:type :boolean}}}
                     :install {:type :boolean}
                     :launch {:type :boolean}}}
              :html5 {:type :object
                      :properties
                      {:architecture {:type :object
                                      :properties
                                      {:js-web {:type :boolean}
                                       :wasm-web {:type :boolean :default true}}}}}
              :windows {:type :object
                        :properties
                        {:platform {:type :enum
                                    ;; derive from bob?
                                    :values ["x86_64-win32" "x86-win32"]}}}}}
    :window {:type :object
             :properties
             {:dimensions {:type :any}
              :split-positions {:type :any}
              :hidden-panes {:type :set :item {:type :keyword}}}}
    :workflow {:type :object
               :properties
               {:load-external-changes-on-app-focus {:type :boolean
                                                     :default true
                                                     :label "Load External Changes on App Focus"}
                :recent-files {:type :array
                               :item {:type :tuple :items [{:type :string} {:type :keyword}]}
                               :scope :project}}}
    :console {:type :object
              :properties
              {:filters {:type :array
                         :item {:type :tuple :items [{:type :string} {:type :boolean}]}}}}
    :run {:type :object
          :properties
          {:instance-count {:type :integer :default 1 :scope :project}
           :selected-target-id {:type :any}
           :manual-target-ip+port {:type :string}
           :quit-on-escape {:type :boolean :label "Escape Quits Game"}
           :simulate-rotated-device (nilable {:type :boolean :scope :project})
           :simulated-resolution {:type :any :scope :project}}}
    :scene {:type :object
            :properties
            {:move-whole-pixels {:type :boolean :default true}}}
    :dev {:type :object
          :properties
          {:custom-engine {:type :any}}}
    :git {:type :object
          :properties
          {:credentials {:type :any :scope :project}}}
    :welcome {:type :object
              :properties
              {:last-opened-project-directory {:type :any}
               :recent-projects {:type :object-of
                                 :key {:type :string}
                                 :val {:type :string}}}}}})

;; region schema validation

(defn valid? [schema value]
  (case (:type schema)
    :any true
    :boolean (boolean? value)
    :string (string? value)
    :keyword (keyword? value)
    :integer (int? value)
    :number (number? value)
    :array (and (vector? value)
                (let [item-schema (:item schema)]
                  (every? #(valid? item-schema %) value)))
    :set (and (set? value)
              (let [item-schema (:item schema)]
                (every? #(valid? item-schema %) value)))
    :object (and (map? value)
                 (every? (fn [[k s]]
                           (let [v (value k ::not-found)]
                             (or (identical? v ::not-found)
                                 (valid? s v))))
                         (:properties schema)))
    :object-of (and (map? value)
                    (let [{:keys [key val]} schema]
                      (every? (fn [[k v]]
                                (and (valid? key k) (valid? val v)))
                              value)))
    :enum (boolean (some #(= value %) (:values schema)))
    :tuple (and (vector? value)
                (let [items (:items schema)
                      n (count items)]
                  (and (= (count value) n)
                       (every? #(valid? (items %) (value %)) (range n)))))
    :one-of (boolean (coll/some #(valid? % value) (:schemas schema)))))

(defn default-value [schema]
  (let [explicit-default (:default schema ::not-found)]
    (if (identical? explicit-default ::not-found)
      (case (:type schema)
        :any nil
        :boolean false
        :string ""
        :keyword nil
        :integer 0
        :number 0.0
        :array []
        :set #{}
        :object {}
        :object-of {}
        :enum ((:values schema) 0)
        :tuple (mapv default-value (:items schema))
        :one-of (default-value ((:schemas schema) 0)))
      explicit-default)))

;; endregion

;; region spec

(defn- default-valid? [schema]
  (let [v (:default schema ::not-found)]
    (or (identical? v ::not-found) (valid? schema v))))
(defmulti type-spec :type)
(s/def ::label string?)
(s/def ::description string?)
(s/def ::default any?)
(s/def ::scope #{:global :project})
(s/def ::type #{:any :boolean :string :keyword :integer :number :array :set :object :object-of :enum :tuple :one-of})
(s/def ::schema
  (s/and
    (s/multi-spec type-spec :type)
    (s/keys :req-un [::type] :opt-un [::label ::description ::default ::scope])
    default-valid?))
(defmethod type-spec :default [_] any?)
(s/def ::item ::schema)
(defmethod type-spec :array [_] (s/keys :req-un [::item]))
(defmethod type-spec :set [_] (s/keys :req-un [::item]))
(s/def ::properties (s/map-of keyword? ::schema))
(defmethod type-spec :object [_] (s/keys :req-un [::properties]))
(s/def ::key ::schema)
(s/def ::val ::schema)
(defmethod type-spec :object-of [_] (s/keys :req-un [::key ::val]))
(s/def ::items (s/coll-of ::schema :kind vector?))
(defmethod type-spec :tuple [_] (s/keys :req-un [::items]))
(s/def ::values (s/coll-of any? :min-count 1 :kind vector?))
(defmethod type-spec :enum [_] (s/keys :req-un [::values]))
(s/def ::schemas (s/coll-of ::schema :kind vector? :distinct true :min-count 2)) ;; 2 alternatives required
(defmethod type-spec :one-of [_] (s/keys :req-un [::schemas]))

(s/def :editor.prefs.preferences/scopes (s/map-of ::scope fs/path? :min-count 1))
(s/def :editor.prefs.preferences/schemas (s/coll-of any? :kind vector? :distinct true :min-count 1))
(s/def ::preferences (s/keys :req-un [:editor.prefs.preferences/scopes
                                      :editor.prefs.preferences/schemas]))

;; endregion

;; region internal global state

(defn- read-config! [path]
  (if (fs/path-exists? path)
    (with-open [rdr (PushbackReader. (io/reader path))]
      (try
        (edn/read {:default fn/constantly-nil} rdr)
        (catch Throwable _ ::not-found)))
    ::not-found))

(defn- array? [x]
  (and (some? x) (.isArray (class x))))

(defn- write-config! [path config]
  (fs/create-path-parent-directories! path)
  (with-open [w (io/writer path)]
    (letfn [(write-contents-indented! [prefix suffix xs indent]
              (let [child-indent (+ indent 2)
                    ^String child-indent-str (apply str (repeat child-indent \space))]
                (.write w ^String prefix)
                (.write w "\n")
                (run!
                  (fn [x]
                    (.write w child-indent-str)
                    (write! x child-indent)
                    (.write w "\n"))
                  xs)
                (.write w ^String (apply str (repeat indent \space)))
                (.write w ^String suffix)))
            (write! [x indent]
              (cond
                (map-entry? x) (do (write! (key x) indent)
                                   (.write w " ")
                                   (write! (val x) indent))
                (set? x) (write-contents-indented! "#{" "}" (sort x) indent)
                (map? x) (write-contents-indented! "{" "}" (sort-by key x) indent)
                (or (vector? x) (array? x)) (write-contents-indented! "[" "]" x indent)
                :else (.write w (pr-str x))))]
      (write! config 0))))

(defn- safe-assoc-in [m p v]
  (cond
    (coll/empty? p) v
    (identical? ::not-found m) (assoc-in {} p v)
    :else (assoc-in m p v)))

(defn- incorporate-updated-storage [{:keys [events storage] :as current-state} updated-storage]
  (let [merged-storage (conj storage updated-storage)]
    (assoc current-state
      :storage (if events
                 ;; new events might have appeared while we were busy with file IO.
                 ;; in this case, we reapply the events to the merged storage since
                 ;; these events might change configs that were reloaded
                 (reduce-kv
                   (fn [acc file-path path->val]
                     (update acc file-path #(reduce-kv safe-assoc-in % path->val)))
                   merged-storage
                   events)
                 merged-storage))))

(defonce io-lock (Object.))

(defn- sync-state! [global-state]
  (when (:events @global-state)
    (when-let [updated-storage
               (locking io-lock
                 (let [{:keys [events storage]} (first (swap-vals! global-state dissoc :events))]
                   (when events
                     (reduce-kv
                       (fn [acc file-path path->val]
                         (let [config (reduce-kv safe-assoc-in (read-config! file-path) path->val)]
                           (write-config! file-path config)
                           (assoc acc file-path config)))
                       storage
                       events))))]
      (swap! global-state incorporate-updated-storage updated-storage))))

(def ^:private ^ScheduledExecutorService sync-executor
  (Executors/newSingleThreadScheduledExecutor
    (reify ThreadFactory
      (newThread [_ r]
        (doto (Thread. r)
          (.setDaemon true)
          (.setName "editor.preferences/sync-executor"))))))

(defn- global-state-watcher [_ global-state old-state new-state]
  (when (and (not (:events old-state))
             (:events new-state))
    (.schedule sync-executor ^Runnable #(sync-state! global-state) 30 TimeUnit/SECONDS)))

(defn- resolve-schema
  "Convert schema to a fully-specified one

  In a fully resolved schema, every schema element up to leaves has a defined
  scope. All nested object schemas are considered branches, and all non-object
  schemas are leaves."
  ([schema]
   (resolve-schema schema :global))
  ([schema default-scope]
   (let [scope (:scope schema default-scope)
         schema (assoc schema :scope scope)]
     (case (:type schema)
       :object (update schema :properties (fn [m]
                                            (coll/pair-map-by key #(resolve-schema (val %) scope) m)))
       schema))))

(def global-state
  ;; global state value is a map with the following keys:
  ;;   :storage     a map from absolute file Path to its prefs map
  ;;   :registry    a map from schema id (anything) to a schema map (schema)
  ;;   :events      a map of events to write, nil when there is nothing to sync;
  ;;                events map is a map with absolute file Path keys, and a map
  ;;                of assoc-in paths to valid config values, i.e.:
  ;;                {java.nio.Path {assoc-in-path value}}
  (let [ret (atom {:storage {}
                   ;; we put default schema into state to use the same schema
                   ;; access pattern, but it is not modifiable (register-schema!
                   ;; disallows using :default key)
                   :registry {:default (s/assert ::schema (resolve-schema default-schema))}})]
    (add-watch ret global-state-watcher global-state-watcher)
    (.addShutdownHook (Runtime/getRuntime) (Thread. #(sync-state! global-state)))
    ret))

(defn- ensure-loaded! [path]
  (when-not (contains? (:storage @global-state) path)
    (let [config (read-config! path)]
      (swap! global-state update :storage assoc path config))))

;; endregion

;; region impl

(defn- lookup-schema-at-path [schema path]
  (reduce (fn [schema k]
            (or (when (= :object (:type schema))
                  ((:properties schema) k))
                (reduced nil)))
          schema
          path))

(defn- lookup-valid-value-at-path [scopes storage schema path]
  (if (= :object (:type schema))
    (coll/pair-map-by
      key
      #(lookup-valid-value-at-path scopes storage (val %) (conj path (key %)))
      (:properties schema))
    (let [value (-> schema :scope scopes storage (get-in path ::not-found))]
      (if (or (identical? value ::not-found)
              (not (valid? schema value)))
        (default-value schema)
        value))))

(defn merge-schemas
  "Similar to clojure.core/merge-with, but for schemas

  Automatically merges nested object schemas

  Args:
    f       schema merge function, will receive 3 args:
            - first conflicting schema
            - second conflicting schema
            - conflict path
            if f returns nil, both schemas are omitted from the output
    a       first schema
    b       second schema
    path    initial schema path"
  ([f a b]
   (merge-schemas f a b []))
  ([f a b path]
   (if (and (= :object (:type a))
            (= :object (:type b)))
     (-> b
         (conj a)
         (assoc :properties (reduce-kv
                              (fn [acc b-k b-v]
                                (let [a-v (acc b-k ::not-found)]
                                  (if (identical? ::not-found a-v)
                                    (assoc acc b-k b-v)
                                    (let [v (merge-schemas f a-v b-v (conj path b-k))]
                                      (if v
                                        (assoc acc b-k v)
                                        (dissoc acc b-k))))))
                              (:properties a)
                              (:properties b))))
     (f a b path))))

(defn difference-schemas
  "Similar to clojure.set/difference, but for schemas

  Automatically diffs nested object schemas

  Args:
    f       callback function for removed entries, will receive 3 args:
            - first conflicting schema
            - second conflicting schema
            - conflict path
    a       first schema
    b       second schema
    path    initial schema path"
  ([f a b]
   (difference-schemas f a b []))
  ([f a b path]
   (if (and (= :object (:type a))
            (= :object (:type b)))
     (let [new-properties (reduce-kv
                            (fn [acc b-k b-v]
                              (let [a-v (acc b-k ::not-found)]
                                (if (identical? ::not-found a-v)
                                  acc
                                  (let [v (difference-schemas f a-v b-v (conj path b-k))]
                                    (if v
                                      (assoc acc b-k v)
                                      (dissoc acc b-k))))))
                            (:properties a)
                            (:properties b))]
       (if (coll/empty? new-properties)
         nil
         (assoc a :properties new-properties)))
     (do (f a b path)
         nil))))

(defn- prefer-first [a _ _] a)

(defn- path-error [path]
  (ex-info (str "No schema defined for prefs path " path)
           {::error :path
            :path path}))

(defn- value-error [path value schema]
  (ex-info (str "Invalid new value at prefs path " path ": " value)
           {::error :value
            :path path
            :value value
            :schema schema}))

(defn- combined-schema-at-path [registry prefs path]
  (let [ret (->> prefs
                 :schemas
                 (e/keep #(some-> (registry %) (lookup-schema-at-path path)))
                 (reduce
                   (fn [acc schema]
                     (if (identical? ::not-found acc)
                       schema
                       (merge-schemas prefer-first acc schema path)))
                   ::not-found))]
    (if (identical? ::not-found ret)
      (throw (path-error path))
      ret)))

(defn- set-value-events [scopes schema path value]
  (if (= :object (:type schema))
    (if (map? value)
      (let [{:keys [properties]} schema]
        (e/mapcat
          (fn [e]
            (let [k (key e)
                  property-path (conj path k)]
              (if-let [property-schema (properties k)]
                (set-value-events scopes property-schema property-path (val e))
                (throw (path-error property-path)))))
          value))
      (throw (value-error path value schema)))
    (if (valid? schema value)
      [[(-> schema :scope scopes) path value]]
      (throw (value-error path value schema)))))

;; endregion

;; region public api

(defn make
  "Construct a preferences map

  Will load the preferences into memory during construction

  kv-args:
    :scopes     a map from scope (:global or :project) to a fs path; optional if
                parent preferences are specified
    :schemas    a vector of schema ids, e.g. :default or id registered with
                `register-schema!` function; optional if parent preferences are
                specified
    :parent     a parent preferences map, defines initial scopes and schemas"
  [& {:keys [scopes schemas parent]}]
  {:post [(s/assert ::preferences %)]}
  (let [scopes (coll/pair-map-by key #(-> % val fs/path .toAbsolutePath (doto ensure-loaded!)) scopes)
        scopes (cond->> (or scopes {}) parent (conj (:scopes parent)))
        schemas (into [] (comp cat (distinct)) [(:schemas parent) schemas])]
    {:scopes scopes
     :schemas schemas}))

(defn get
  "Get a value from preferences at a specified get-in path

  Will only return values specified in the combined schema. Does not perform
  file IO.

  Using [] as a path will return the full preference map"
  [prefs path]
  {:pre [(vector? path)]}
  (let [{:keys [registry storage]} @global-state
        schema (combined-schema-at-path registry prefs path)]
    (lookup-valid-value-at-path (:scopes prefs) storage schema path)))

(defn set!
  "Set a value in preferences at a specified assoc-in path

  The new value must satisfy the combined schema. Does not perform file IO.

  Using [] as a path allows changing the whole preference state"
  [prefs path value]
  (let [{:keys [scopes]} prefs]
    (swap! global-state (fn [{:keys [registry] :as m}]
                          (let [schema (combined-schema-at-path registry prefs path)]
                            (->> value
                                 (set-value-events scopes schema path)
                                 (reduce
                                   (fn [acc [file-path config-path value]]
                                     (-> acc
                                         (update-in [:storage file-path] safe-assoc-in config-path value)
                                         (assoc-in [:events file-path config-path] value)))
                                   m)))))
    nil))

(defn schema
  "Get a preference schema at a specified get-in path"
  [prefs path]
  (combined-schema-at-path (:registry @global-state) prefs path))

(defn register-schema!
  "Register a new schema, e.g. a project-specific one"
  [id schema]
  {:pre [(not= id :default) (s/assert ::schema schema)]}
  (let [schema (resolve-schema schema)]
    (swap! global-state assoc-in [:registry id] schema)
    nil))

(defn unregister-schema!
  "Unregister a previously registered schema"
  [id]
  {:pre [(not= id :default)]}
  (swap! global-state update :registry dissoc id)
  nil)

(defn global
  "Return a Defold-specific user-level prefs

  Any attempt to get project-scoped values will return defaults"
  ([]
   (global (fs/path
             (case (os/os)
               :macos (fs/evaluate-path "~/Library/Preferences")
               :linux (some fs/evaluate-path ["$XDG_CONFIG_HOME" "~/.config"])
               :win32 (some fs/evaluate-path ["$APPDATA" "~/AppData/Roaming"]))
             "Defold"
             "prefs.editor_settings")))
  ([prefs-path]
   (make :scopes {:global prefs-path}
         :schemas [:default])))

(defn project
  "Return preferences scoped to a specific Defold project

  Use `register-project-schema!` to define project-specific schema (will use
  real path of the project root as a schema id)"
  ([project-path]
   (project project-path (global)))
  ([project-path parent-prefs]
   (let [real-path (fs/real-path project-path)]
     (make :parent parent-prefs
           :scopes {:project (fs/path real-path ".editor_settings")}
           :schemas [real-path]))))

(defn register-project-schema!
  "Register a project-specific preference schema

  Uses real path of the project root of the project root as a schema id"
  [project-path schema]
  (register-schema! (fs/real-path project-path) schema))

(defn sync!
  "Immediately write all unsaved preference changes into files"
  []
  (sync-state! global-state))

;; endregion

;; TODO: remove migration from legacy prefs after sufficient time has passed (e.g. after 2025-10-15)

;; region migration

(defn- migrate! [prefs legacy-key->path]
  (let [legacy-prefs (.node (Preferences/userRoot) "defold")
        not-found "editor.prefs/not-found"
        {:keys [scopes schemas]} prefs
        {:keys [registry storage]} @global-state]
    (->> legacy-key->path
         (e/keep (fn [[legacy-key path]]
                   (let [v (.get legacy-prefs legacy-key not-found)]
                     (when (and (not (identical? not-found v))
                                ;; is fresh file?
                                (->> schemas
                                     (e/keep #(some-> (registry %) (lookup-schema-at-path path)))
                                     (e/map #(-> % :scope scopes storage))
                                     (coll/some #(identical? ::not-found %))
                                     boolean))
                       (coll/pair path (transit/read (transit/reader (ByteArrayInputStream. (.getBytes v StandardCharsets/UTF_8)) :json)))))))
         (reduce #(assoc-in %1 (key %2) (val %2)) {})
         ;; since `set!` is a special form, we need to use a fully-qualified reference
         (editor.prefs/set! prefs []))))

(defn migrate-global-prefs!
  "Migrate global prefs from the old prefs storage

  Only performs migration if global prefs file didn't exist before. This means
  you should perform migration before modifying the prefs"
  [global-prefs]
  (migrate!
    global-prefs
    {"adb-path" [:tools :adb-path]
     "ios-deploy-path" [:tools :ios-deploy-path]
     "external-changes-load-on-app-focus" [:workflow :load-external-changes-on-app-focus]
     "open-bundle-target-folder" [:bundle :open-output-directory]
     "asset-browser-track-active-tab?" [:asset-browser :track-active-tab]
     "general-quit-on-esc" [:run :quit-on-escape]
     "custom-keymap-path" [:input :keymap-path]
     "code-custom-editor" [:code :custom-editor]
     "code-open-file" [:code :open-file]
     "code-open-file-at-line" [:code :open-file-at-line]
     "code-editor-font-name" [:code :font :name]
     "code-editor-font-size" [:code :font :size]
     "code-editor-visible-indentation-guides" [:code :visibility :indentation-guides]
     "code-editor-visible-minimap" [:code :visibility :minimap]
     "code-editor-visible-whitespace" [:code :visibility :whitespace]
     "extensions-server" [:extensions :build-server]
     "extensions-server-headers" [:extensions :build-server-headers]
     "window-dimensions" [:window :dimensions]
     "split-positions" [:window :split-positions]
     "hidden-panes" [:window :hidden-panes]
     "console-filters" [:console :filters]
     "selected-target-id" [:run :selected-target-id]
     "manual-target-ip+port" [:run :manual-target-ip+port]
     "scene-move-whole-pixels?" [:scene :move-whole-pixels]
     "dev-custom-engine" [:dev :custom-engine]
     "open-project-directory" [:welcome :last-opened-project-directory]
     "recent-project-entries" [:welcome :recent-projects]}))

(defn migrate-project-prefs!
  "Migrate project prefs from the old prefs storage

  Only performs migration if project prefs file didn't exist before. This means
  you should perform migration before modifying the prefs"
  [project-prefs]
  (let [suffix (str "-" (hash (str (.getParent ^Path (:project (:scopes project-prefs))))))]
    (migrate! project-prefs (e/concat
                              ;; move from global to project scope
                              {"search-in-files-term" [:search-in-files :term]
                               "search-in-files-exts" [:search-in-files :exts]
                               "search-in-files-include-libraries" [:search-in-files :include-libraries]
                               "open-assets-term" [:open-assets :term]
                               "code-editor-find-term" [:code :find :term]
                               "code-editor-find-replacement" [:code :find :replacement]
                               "code-editor-find-whole-word" [:code :find :whole-word]
                               "code-editor-find-case-sensitive" [:code :find :case-sensitive]
                               "code-editor-find-wrap" [:code :find :wrap]
                               "general-enable-texture-compression" [:build :texture-compression]
                               "general-lint-on-build" [:build :lint-code]
                               "bundle-variant" [:bundle :variant]
                               "bundle-texture-compression" [:bundle :texture-compression]
                               "bundle-generate-debug-symbols?" [:bundle :debug-symbols]
                               "bundle-generate-build-report?" [:bundle :build-report]
                               "bundle-publish-live-update-content?" [:bundle :liveupdate]
                               "bundle-contentless?" [:bundle :contentless]
                               "bundle-android-keystore" [:bundle :android :keystore]
                               "bundle-android-keystore-pass" [:bundle :android :keystore-pass]
                               "bundle-android-key-pass" [:bundle :android :key-pass]
                               "bundle-android-architecture-32bit?" [:bundle :android :architecture :armv7-android]
                               "bundle-android-architecture-64bit?" [:bundle :android :architecture :arm64-android]
                               "bundle-android-bundle-format" [:bundle :android :format]
                               "bundle-android-install-app?" [:bundle :android :install]
                               "bundle-android-launch-app?" [:bundle :android :launch]
                               "bundle-macos-architecture-x86_64?" [:bundle :macos :architecture :x86_64-macos]
                               "bundle-macos-architecture-arm64?" [:bundle :macos :architecture :arm64-macos]
                               "bundle-ios-sign-app?" [:bundle :ios :sign]
                               "bundle-ios-code-signing-identity" [:bundle :ios :code-signing-identity]
                               "bundle-ios-provisioning-profile" [:bundle :ios :provisioning-profile]
                               "bundle-ios-architecture-64bit?" [:bundle :ios :architecture :arm64-ios]
                               "bundle-ios-architecture-simulator?" [:bundle :ios :architecture :x86_64-ios]
                               "bundle-ios-install-app?" [:bundle :ios :install]
                               "bundle-ios-launch-app?" [:bundle :ios :launch]
                               "bundle-html5-architecture-js-web?" [:bundle :html5 :architecture :js-web]
                               "bundle-html5-architecture-wasm-web?" [:bundle :html5 :architecture :wasm-web]
                               "bundle-windows-platform" [:bundle :windows :platform]
                               "project-git-credentials" [:git :credentials]}
                              ;; these prefs already used project scope
                              (e/map #(coll/pair (str (key %) suffix) (val %))
                                     {"bundle-output-directory" [:bundle :output-directory]
                                      "recent-files-by-workspace-root" [:workflow :recent-files]
                                      "instance-count" [:run :instance-count]
                                      "simulate-rotated-device" [:run :simulate-rotated-device]
                                      "simulated-resolution" [:run :simulated-resolution]})))))

;; end region
