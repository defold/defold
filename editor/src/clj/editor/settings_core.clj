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

(ns editor.settings-core
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.string :as s]
            [editor.system :as system]
            [editor.url :as url]
            [util.text-util :as text-util])
  (:import [java.io BufferedReader PushbackReader Reader StringReader]))

(set! *warn-on-reflection* true)

(defn setting-index [settings path]
  (first (keep-indexed (fn [index item] (when (= (:path item) path) index)) settings)))

(defn remove-setting [settings path]
  (if-let [index (setting-index settings path)]
    (vec (concat (subvec settings 0 index) (subvec settings (inc index))))
    settings))

(defn clear-setting [settings path]
  (if-let [index (setting-index settings path)]
    (update settings index dissoc :value)
    settings))

(defn get-setting [settings path]
  (when-let [index (setting-index settings path)]
    (:value (nth settings index))))

(defn set-setting [settings path value]
  (if-let [index (setting-index settings path)]
    (assoc-in settings [index :value] value)
    (conj settings {:path path :value value})))

(defn- non-blank [vals]
  (remove s/blank? vals))

(defn- trimmed [lines]
  (map s/trim lines))

(defn- read-setting-lines [setting-reader]
  (non-blank (trimmed (line-seq setting-reader))))

(defn resource-reader
  ^Reader [resource-name]
  (io/reader (io/resource resource-name) :encoding "UTF-8"))

(defn pushback-reader
  ^PushbackReader [reader]
  (PushbackReader. reader))

(defn string-reader
  ^Reader [content]
  (BufferedReader. (StringReader. content)))

(defn- empty-parse-state []
  {:current-category nil :settings []})

(defn- parse-category-line [{:keys [current-category settings] :as parse-state} line]
  (when-let [[_ new-category] (re-find #"^\s*\[([^\]]*)\]" line)]
    (assoc parse-state :current-category new-category)))

(defn- parse-setting-line [parse-state line]
  (when-let [[_ key value] (seq (map s/trim (re-find #"([^=]+)=(.*)" line)))]
    (let [[cleaned-path index-string] (non-blank (s/split key #"\#"))]
      (when-let [setting-path (seq (non-blank (s/split cleaned-path #"\.")))]
        (let [full-path (into [(:current-category parse-state)] setting-path)]
          ;; For non-list values, the value will be a string.
          ;; For list values, we build up a vector of strings that we then join
          ;; into comma-separated strings in the parse-state->settings function.
          (if (nil? index-string)
            (update parse-state :settings conj {:path full-path :value value})
            (update parse-state :settings (fn [settings]
                                            (let [entries (or (get-setting settings full-path) [])]
                                              (set-setting settings full-path (conj entries value)))))))))))

(defn- parse-error [line]
  (throw (Exception. (format "Invalid setting line: %s" line))))

(defn- parse-state->settings [parse-state]
  ;; After parsing, list values will be vectors of strings.
  ;; Join them into comma-separated string values.
  (into []
        (map (fn [setting]
               (update setting :value (fn [value]
                                        (if (vector? value)
                                          (text-util/join-comma-separated-string value)
                                          value)))))
        (:settings parse-state)))

(defn parse-settings [reader]
  (parse-state->settings (reduce
                          (fn [parse-state line]
                            (or (parse-category-line parse-state line)
                                (parse-setting-line parse-state line)
                                (parse-error line)))
                          (empty-parse-state)
                          (read-setting-lines reader))))

(defn inject-jvm-properties [^String raw-setting-value]
  ;; Replace patterns such as {{defold.extension.spine.url}} with JVM property values.
  (s/replace
    raw-setting-value
    #"\{\{(.+?)\}\}" ; Match the text inside the an {{...}} expression.
    (fn [[_ jvm-property-key]]
      (or (System/getProperty jvm-property-key)
          (throw (ex-info (format "Required JVM property `%s` is not defined."
                                  jvm-property-key)
                          {:jvm-property-key jvm-property-key
                           :raw-setting-value raw-setting-value}))))))

(defmulti parse-setting-value (fn [meta-setting ^String raw] (:type meta-setting)))

(defmethod parse-setting-value :string [_ raw]
  raw)

(defmethod parse-setting-value :boolean [_ raw]
  ;; this is roughly how the old editor does it, rather than != 0.
  (= raw "1"))

(defmethod parse-setting-value :integer [_ raw]
  ;; Parsed as 32-bit integer in the engine runtime.
  (Integer/parseInt raw))

(defmethod parse-setting-value :number [_ raw]
  ;; Parsed as 32-bit float in the engine runtime.
  (Float/parseFloat raw))

(defmethod parse-setting-value :resource [_ raw]
  raw)

(defmethod parse-setting-value :file [_ raw]
  raw)

(defmethod parse-setting-value :directory [_ raw]
  raw)

;; In dev builds, we replace patterns such as {{defold.extension.spine.url}}
;; with JVM property values in dev builds. This ensures we can use a specific
;; branch of an extension in the integration tests as we develop new features.
(if (system/defold-dev?)
  (defmethod parse-setting-value :url [_ raw]
    (some-> raw inject-jvm-properties url/try-parse))
  (defmethod parse-setting-value :url [_ raw]
    (some-> raw url/try-parse)))

(def ^:private default-list-element-meta-setting {:type :string})

(defn- parse-list-setting-value [meta-setting raw]
  (when raw
    (let [element-meta-setting (get meta-setting :element default-list-element-meta-setting)]
      (into []
            (keep #(parse-setting-value element-meta-setting %))
            (text-util/parse-comma-separated-string raw)))))

(defmethod parse-setting-value :list [meta-setting raw]
  (parse-list-setting-value meta-setting raw))

(defmethod parse-setting-value :comma-separated-list [meta-setting raw]
  (parse-list-setting-value meta-setting raw))

(def ^:private type-defaults
  {:list []
   :comma-separated-list []
   :string ""
   :boolean false
   :integer (int 0) ; Parsed as 32-bit integer in the engine runtime.
   :number (float 0.0)}) ; Parsed as 32-bit float in the engine runtime.

(defn- sanitize-default [default type]
  (case type
    :integer (int default) ; Parsed as 32-bit integer in the engine runtime.
    :number (float default) ; Parsed as 32-bit float in the engine runtime.
    default))

(defn- ensure-type-defaults [meta-info]
  (update meta-info :settings
          (fn [settings]
            (mapv (fn [{:keys [type] :as meta-setting}]
                    (update meta-setting :default
                            (fn [default]
                              (if (some? default)
                                (sanitize-default default type)
                                (type-defaults type)))))
                  settings))))

(declare render-raw-setting-value)

(defn- add-to-from-string [meta-info]
  (update meta-info :settings
          (fn [settings]
            (mapv (fn [meta-setting]
                    (if (contains? meta-setting :options)
                      (assoc meta-setting
                        :from-string #(parse-setting-value meta-setting %)
                        :to-string #(render-raw-setting-value meta-setting %))
                      meta-setting))
                  settings))))

(defn remove-to-from-string [meta-info]
  (update meta-info :settings
          (fn [settings]
            (mapv (fn [meta-setting]
                    (if (contains? meta-setting :options)
                      (dissoc meta-setting :from-string :to-string)
                      meta-setting))
                  settings))))

(defn finalize-meta-info [meta-info]
  (-> meta-info
      ensure-type-defaults
      add-to-from-string))

(defn load-meta-info [reader]
  (finalize-meta-info (edn/read reader)))

(defn label [key]
  (->> (s/split (name key) #"(_|\s+)")
       (mapv s/capitalize)
       (s/join " ")))

(defn- make-meta-settings-for-unknown [meta-settings settings]
  (let [known-settings (into #{} (map :path) meta-settings)]
    (into []
          (comp
            (map :path)
            (remove known-settings)
            (map (fn [setting-path]
                   {:path setting-path
                    :type :string
                    :help (label (last setting-path))
                    :unknown-setting? true})))
          settings)))

(defn add-meta-info-for-unknown-settings [meta-info settings]
  (update meta-info :settings #(concat % (make-meta-settings-for-unknown % settings))))

(defn make-meta-settings-map [meta-settings]
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
  (when-some [meta-setting (meta-settings-map path)]
    (update setting :value
            #(do (->> %
                     (parse-setting-value meta-setting)
                     (sanitize-value meta-setting))))))

(defn sanitize-settings [meta-settings settings]
  (vec (map (partial sanitize-setting (make-meta-settings-map meta-settings)) settings)))

(defn make-default-settings [meta-settings]
  (mapv (fn [meta-setting]
          {:path (:path meta-setting)
           :value (:default meta-setting)})
       meta-settings))

(def setting-category (comp first :path))

(defn presentation-category [setting]
  (or (:presentation-category setting)
      (setting-category setting)))

(defn- category-order [settings]
  (distinct (map setting-category settings)))

(defn- category-grouped-settings [settings]
  (group-by setting-category settings))

(defn- setting->str [{:keys [path value]}]
  (let [key (s/join "." (rest path))]
    (str key " = " value)))

(defn- category->str [category settings]
  (s/join "\n" (cons (str "[" category "]") (map setting->str settings))))

(defn- split-multi-line-setting-at-path [settings setting-path]
  (let [comma-separated-setting (get-setting settings setting-path)]
    (if (empty? comma-separated-setting)
      settings
      (let [list-element-strings (text-util/parse-comma-separated-string comma-separated-setting)
            cleaned-settings (remove-setting settings setting-path)
            [category key] setting-path]
        (reduce-kv
          (fn [settings index list-element-string]
            (set-setting settings [category (str key "#" index)] list-element-string))
          cleaned-settings
          list-element-strings)))))

(defn- split-multi-line-settings [settings meta-settings]
  (transduce (keep (fn [meta-setting]
                     (when (= :list (:type meta-setting))
                       (:path meta-setting))))
             (completing split-multi-line-setting-at-path)
             settings
             meta-settings))

(defn settings->str
  ^String [settings meta-settings list-format]
  (let [split-settings (case list-format
                         :comma-separated-list settings
                         :multi-line-list (split-multi-line-settings (vec settings) meta-settings))
        cat-order (category-order split-settings)
        cat-grouped-settings (category-grouped-settings split-settings)]

    ;; Here we interleave categories with \n\n rather than join to make sure the file also ends with
    ;; two consecutive newlines. This is purely to avoid whitespace diffs when loading a project
    ;; created in the old editor and saving.
    (s/join (interleave (map #(category->str % (cat-grouped-settings %))
                             cat-order)
                        (repeat "\n\n")))))

(defmulti render-raw-setting-value (fn [meta-setting value] (:type meta-setting)))

(defmethod render-raw-setting-value :default [_ value]
  (str value))

(defmethod render-raw-setting-value :boolean [_ value]
  (if value "1" "0"))

(defmethod render-raw-setting-value :resource [{:keys [preserve-extension]} value]
  (if (and (not (s/blank? value))
           (not preserve-extension))
    (str value "c")
    value))

(defmulti quoted-list-element-setting? (fn [meta-setting] (:type meta-setting)))

(defmethod quoted-list-element-setting? :default [_] false)

(defmethod quoted-list-element-setting? :string [_] true)

(defmethod quoted-list-element-setting? :resource [_] true)

(defmethod quoted-list-element-setting? :file [_] true)

(defmethod quoted-list-element-setting? :directory [_] true)

(defmethod quoted-list-element-setting? :url [_] true)

(defn- render-raw-list-setting-value [meta-setting value]
  (when (seq value)
    (let [element-meta-setting (get meta-setting :element default-list-element-meta-setting)
          string-values (map #(render-raw-setting-value element-meta-setting %) value)]
      (if (quoted-list-element-setting? element-meta-setting)
        (text-util/join-comma-separated-string string-values)
        (s/join ", " string-values)))))

(defmethod render-raw-setting-value :list [meta-setting value]
  (render-raw-list-setting-value meta-setting value))

(defmethod render-raw-setting-value :comma-separated-list [meta-setting value]
  (render-raw-list-setting-value meta-setting value))

(defn make-settings-map [settings]
  (into {} (map (juxt :path :value) settings)))

(defn get-default-setting [meta-settings path]
  (when-let [index (setting-index meta-settings path)]
    (:default (nth meta-settings index))))

(defn get-setting-or-default [meta-settings settings path]
  (or (get-setting settings path)
      (get-default-setting meta-settings path)))

(defn get-meta-setting
  [meta-settings path]
  (when-some [index (setting-index meta-settings path)]
    (nth meta-settings index)))

(defn set-raw-setting [settings meta-settings path value]
  (let [meta-setting (get-meta-setting meta-settings path)]
    (if (= value (:default meta-setting))
      (clear-setting settings path)
      (let [to-string (:to-string meta-setting)
            string-value (if to-string
                           (to-string value)
                           (render-raw-setting-value meta-setting value))]
        (set-setting settings path string-value)))))

(defn settings-with-value [settings]
  (filter #(contains? % :value) settings))

(defn raw-settings-search-fn
  ([search-string]
   (text-util/search-string->re-pattern search-string :case-insensitive))
  ([raw-settings re-pattern]
   (into []
         (keep (fn [{:keys [value] :as raw-setting}]
                 (some-> value
                         (text-util/string->text-match re-pattern)
                         (assoc
                           :match-type :match-type-setting
                           :value value
                           :path (:path raw-setting)))))
         raw-settings)))
