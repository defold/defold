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

(ns editor.lua
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.spec.alpha :as s]
            [clojure.string :as string]
            [editor.code-completion :as code-completion]
            [editor.editor-extensions.docs :as ext-docs]
            [editor.lua-completion :as lua-completion]
            [editor.protobuf :as protobuf]
            [internal.util :as util]
            [util.coll :refer [pair]]
            [util.eduction :as e])
  (:import [com.dynamo.scriptdoc.proto ScriptDoc$Document]
           [java.net URI]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def ^:private docs
  ["base" "bit" "buffer" "b2d" "b2d.body" "builtins" "camera" "collectionfactory"
   "collectionproxy" "coroutine" "crash" "debug" "factory" "go" "gui" "graphics"
   "html5" "http" "image" "io" "json" "label" "liveupdate" "math" "model" "msg"
   "os" "package" "particlefx" "physics" "profiler" "render" "resource"
   "socket" "sound" "sprite" "string" "sys" "table" "tilemap" "timer" "vmath"
   "window" "zlib" "types"])

(defn- sdoc-path [doc]
  (format "doc/%s_doc.sdoc" doc))

(defn- load-sdoc [doc-name]
  (:elements (protobuf/read-map-with-defaults ScriptDoc$Document (io/resource (sdoc-path doc-name)))))

(defn make-completion-map
  "Make a completion map from reducible of ns path + completion tuples

  Args:
    ns-path       a vector of strings that defines a nested context for the
                  completion, e.g. [\"socket\" \"dns\"] with the completion
                  \"toip\" defines a completion socket.dns.toip
    completion    a completion map

  Returns a map from completion context string (e.g. \"socket.dns\") to
  available completions for that context."
  [ns-path+completions]
  (letfn [(ensure-modules [acc ns-path]
            (reduce (fn [acc module-path]
                      (let [parent-path (pop module-path)
                            module-name (peek module-path)
                            k [:module module-name]]
                        (update acc (string/join "." parent-path)
                                (fn [m]
                                  (if (contains? m k)
                                    m
                                    (assoc m k (code-completion/make module-name :type :module)))))))
                    acc
                    (drop 1 (reductions conj [] ns-path))))]
    (let [context->key->completion
          (reduce
            (fn [acc [ns-path completion]]
              (-> acc
                  (ensure-modules ns-path)
                  (update (string/join "." ns-path) assoc [(:type completion) (:display-string completion)] completion)))
            {}
            ns-path+completions)]
      (into {}
            (map (fn [[context key->completion]]
                   [context (vec (sort-by :display-string (vals key->completion)))]))
            context->key->completion))))

(s/def ::documentation
  (s/map-of string? (s/coll-of ::code-completion/completion)))

(defn- defold-documentation []
  {:post [(s/assert ::documentation %)]}
  (make-completion-map
    (eduction
      (mapcat (fn [doc-name]
                (eduction
                  (map #(pair doc-name %))
                  (load-sdoc doc-name))))
      (map (fn [[doc-name raw-element]]
             (let [raw-name (:name raw-element)
                   name-parts (string/split raw-name #"\.")
                   ns-path (pop name-parts)
                   {:keys [type parameters] :as el} (assoc raw-element :name (peek name-parts))
                   base-url (URI. (str "https://defold.com/ref/" doc-name))
                   site-url (str "#"
                                 (string/replace
                                   (str
                                     raw-name
                                     (when (= :function type)
                                       (str ":" (string/join "-" (mapv :name parameters)))))
                                   #"[\"#*]" ""))]
               [ns-path (lua-completion/make el :base-url base-url :url site-url)])))
      docs)))

(def logic-keywords #{"and" "or" "not"})

(def self-keyword #{"self"})

(def control-flow-keywords #{"break" "do" "else" "for" "if" "elseif" "return" "then" "repeat" "while" "until" "end" "function"
                             "local" "goto" "in"})

(def defold-keywords #{"final" "init" "on_input" "on_message" "on_reload" "update" "acquire_input_focus" "disable" "enable"
                       "release_input_focus" "request_transform" "set_parent" "transform_response"})

(def lua-constants #{"nil" "false" "true"})

(def all-keywords
  (set/union logic-keywords
             self-keyword
             control-flow-keywords
             defold-keywords))

(def defold-docs (defold-documentation))
(def preinstalled-modules (into #{} (remove #{""} (keys defold-docs))))

(def base-globals
  (into #{"coroutine" "package" "string" "table" "math" "io" "file" "os" "debug"}
        (map :name)
        (load-sdoc "base")))

(defn extract-globals-from-completions [completions]
  (into #{}
        (comp
          (remove #(#{:message :property} (:type %)))
          (map :name)
          (remove #(or (= "" %)
                       (string/includes? % ":")
                       (contains? base-globals %))))
        (get completions "")))

(def defined-globals
  (extract-globals-from-completions defold-docs))

(defn- lua-base-documentation []
  {:post [(s/assert ::documentation %)]}
  {"" (into []
            (util/distinct-by :display-string)
            (concat (map #(assoc % :type :snippet)
                         (-> (io/resource "lua-base-snippets.edn")
                             slurp
                             edn/read-string))
                    (map #(code-completion/make % :type :constant)
                         lua-constants)
                    (map #(code-completion/make % :type :keyword)
                         all-keywords)))})

(def std-libs-docs (lua-base-documentation))

(defn lua-module->path [module]
  (str "/" (string/replace module #"\." "/") ".lua"))

(defn path->lua-module [path]
  (-> (if (string/starts-with? path "/") (subs path 1) path)
      (string/replace #"/" ".")
      (FilenameUtils/removeExtension)))

(defn lua-module->build-path [module]
  (str (lua-module->path module) "c"))

(defn- make-completion-info-completions [vars functions module-alias]
  (eduction
    cat
    [(eduction
       (map (fn [var-name]
              [[] (code-completion/make var-name :type :variable)]))
       vars)
     (eduction
       (map (fn [[qualified-name {:keys [params]}]]
              (let [name-parts (string/split qualified-name #"\.")
                    ns-path (pop name-parts)
                    ns-path (if (and module-alias (pos? (count ns-path)))
                              (assoc ns-path 0 module-alias)
                              ns-path)
                    name (peek name-parts)]
                [ns-path (code-completion/make
                           name
                           :type :function
                           :display-string (str name "(" (string/join ", " params) ")")
                           :insert (str name
                                        "("
                                        (->> params
                                             (map-indexed #(format "${%s:%s}" (inc %1) %2))
                                             (string/join ", "))
                                        ")"))])))
       functions)]))

(defn- make-ast-completions [local-completion-info required-completion-infos]
  (make-completion-map
    (eduction
      cat
      [(make-completion-info-completions
         (set/union (:vars local-completion-info) (:local-vars local-completion-info))
         (merge (:functions local-completion-info) (:local-functions local-completion-info))
         nil)
       (let [module->completion-info (into {} (map (juxt :module identity)) required-completion-infos)]
         (eduction
           (mapcat (fn [[alias module]]
                     (let [completion-info (module->completion-info module)]
                       (make-completion-info-completions (:vars completion-info)
                                                         (:functions completion-info)
                                                         alias))))
           (:requires local-completion-info)))])))

(defn combine-completions
  [local-completion-info required-completion-infos script-intelligence-completions]
  (merge-with (fn [dest new]
                (let [taken-display-strings (into #{} (map :display-string) dest)]
                  (into dest
                        (comp (remove #(contains? taken-display-strings (:display-string %)))
                              (util/distinct-by :display-string))
                        new)))
              defold-docs
              std-libs-docs
              script-intelligence-completions
              (make-ast-completions local-completion-info required-completion-infos)))

(def editor-completions
  (->> (ext-docs/editor-script-docs)
       (e/map
         (fn [doc]
           (let [name-parts (string/split (:name doc) #"\.")]
             [(pop name-parts) (lua-completion/make (assoc doc :name (peek name-parts)))])))
       (make-completion-map)))
