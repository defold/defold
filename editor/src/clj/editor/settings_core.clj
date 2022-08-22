;; Copyright 2020-2022 The Defold Foundation
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

(defn- parse-setting-line [{:keys [current-category settings] :as parse-state} line]
  (when-let [[_ key val] (seq (map s/trim (re-find #"([^=]+)=(.*)" line)))]
    (let [cleaned-path (first (seq (non-blank (s/split key #"\#"))))]
      (when-let [setting-path (seq (non-blank (s/split cleaned-path #"\.")))]
        (let [full-path (into [current-category] setting-path)]
          (if-let [existing-value (get-setting settings full-path)]
            (update parse-state :settings set-setting full-path (str existing-value "," val))
            (update parse-state :settings conj {:path full-path :value val})))))))

(defn- parse-error [line]
  (throw (Exception. (format "Invalid setting line: %s" line))))

(defn- parse-state->settings [{:keys [settings]}]
  settings)

(defn parse-settings [reader]
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

(defmethod parse-setting-value :file [_ raw]
  raw)

(defmethod parse-setting-value :directory [_ raw]
  raw)

(defmethod parse-setting-value :comma-separated-list [_ raw]
  (when raw
    (into [] (text-util/parse-comma-separated-string raw))))

(def ^:private type-defaults
  {:string ""
   :boolean false
   :integer 0
   :number 0.0
   :resource nil
   :file nil
   :directory nil})

(defn- add-type-defaults [meta-info]
  (update-in meta-info [:settings]
             (partial map (fn [setting] (update setting :default #(if (nil? %) (type-defaults (:type setting)) %))))))

(declare render-raw-setting-value)

(defn- add-to-from-string [meta-info]
  (update-in meta-info [:settings]
             (fn [settings]
               (map (fn [meta-setting]
                      (if (contains? meta-setting :options)
                        (let [type (:type meta-setting)]
                          (assoc meta-setting
                                 :from-string #(parse-setting-value type %)
                                 :to-string #(render-raw-setting-value meta-setting %)))
                        meta-setting))
                    settings))))

(defn remove-to-from-string [meta-info]
  (update-in meta-info [:settings]
             (fn [settings]
               (map (fn [meta-setting]
                      (if (contains? meta-setting :options)
                        (let [type (:type meta-setting)]
                          (dissoc meta-setting :from-string :to-string))
                        meta-setting))
                    settings))))

(defn load-meta-info [reader]
  (-> (add-type-defaults (edn/read reader))
      (add-to-from-string)))

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
  (when-let [{:keys [type] :as meta-setting} (meta-settings-map path)]
    (update setting :value
            #(do (->> %
                     (parse-setting-value type)
                     (sanitize-value meta-setting))))))

(defn sanitize-settings [meta-settings settings]
  (vec (map (partial sanitize-setting (make-meta-settings-map meta-settings)) settings)))

(defn make-default-settings [meta-settings]
  (mapv (fn [meta-setting]
         {:path (:path meta-setting) :value (:default meta-setting)})
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

(defn split-multi-line-setting [category key settings]
  (let [path [category key]]
    (if-let [comma-separated-setting (not-empty (get-setting settings path))]
      (let [comma-separated-setting-parts (non-blank (s/split comma-separated-setting #","))
            comma-separated-setting-count (count comma-separated-setting-parts)
            cleaned-settings (remove-setting settings path)]
        (reduce
          (fn [settings i]
            (set-setting settings [category (str key "#" i)] (nth comma-separated-setting-parts i)))
          cleaned-settings
          (range 0 comma-separated-setting-count)))
      settings)))

(defn settings->str [settings]
  (let [split-settings (split-multi-line-setting "project" "dependencies" (vec settings))
        cat-order (category-order split-settings)
        cat-grouped-settings (category-grouped-settings split-settings)]

    ;; Here we interleave categories with \n\n rather than join to make sure the file also ends with
    ;; two consecutive newlines. This is purely to avoid whitespace diffs when loading a project
    ;; created in the old editor and saving.
    (s/join (interleave (map #(category->str % (cat-grouped-settings %)) cat-order) (repeat "\n\n")))))

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

(defmethod render-raw-setting-value :comma-separated-list [_ value]
  (when (seq value) (text-util/join-comma-separated-string value)))

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


(defn settings-with-value [settings]
  (filter #(contains? % :value) settings))
