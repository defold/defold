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

(ns editor.editor-extensions.validation
  (:refer-clojure :exclude [ensure])
  (:require [clojure.spec.alpha :as s]
            [clojure.string :as str]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.util :as util])
  (:import [clojure.lang MultiFn]
           [java.util List Map]
           [org.luaj.vm2 LuaError LuaFunction LuaValue]))

(set! *warn-on-reflection* true)

(defn lua-fn? [x]
  (instance? LuaFunction x))

(defn lua-value? [x]
  (instance? LuaValue x))

(defn resource-path? [x]
  (and (string? x) (string/starts-with? x "/")))

(def node-id? g/node-id?)

(s/def ::node-id-or-path
  (s/or :internal-id node-id? :resource-path resource-path?))

;; editor script module

(s/def :editor.extensions.validation.module/get_commands lua-fn?)
(s/def :editor.extensions.validation.module/on_build_started lua-fn?)
(s/def :editor.extensions.validation.module/on_build_finished lua-fn?)
(s/def :editor.extensions.validation.module/on_bundle_started lua-fn?)
(s/def :editor.extensions.validation.module/on_bundle_finished lua-fn?)
(s/def :editor.extensions.validation.module/on_target_launched lua-fn?)
(s/def :editor.extensions.validation.module/on_target_terminated lua-fn?)
(s/def ::module
  (s/keys :opt-un [:editor.extensions.validation.module/get_commands
                   :editor.extensions.validation.module/on_build_started
                   :editor.extensions.validation.module/on_build_finished
                   :editor.extensions.validation.module/on_bundle_started
                   :editor.extensions.validation.module/on_bundle_finished
                   :editor.extensions.validation.module/on_target_launched
                   :editor.extensions.validation.module/on_target_terminated]))

;; action

(s/def :editor.extensions.validation.action/action #{"set" "shell"})
(s/def :editor.extensions.validation.action/node_id ::node-id-or-path)
(s/def :editor.extensions.validation.action/property string?)
(s/def :editor.extensions.validation.action/value any?)
(s/def :editor.extensions.validation.action/command (s/coll-of string?))
(defmulti action-spec :action)
(defmethod action-spec "set" [_]
  (s/keys :req-un [:editor.extensions.validation.action/action
                   :editor.extensions.validation.action/node_id
                   :editor.extensions.validation.action/property
                   :editor.extensions.validation.action/value]))
(defmethod action-spec "shell" [_]
  (s/keys :req-un [:editor.extensions.validation.action/action
                   :editor.extensions.validation.action/command]))
(s/def ::action (s/multi-spec action-spec :action))
(s/def ::actions (s/coll-of ::action))

;; menu commands

(s/def :editor.extensions.validation.command/label string?)
(s/def :editor.extensions.validation.command/locations (s/coll-of #{"Edit" "View" "Assets" "Outline"} :distinct true :min-count 1))
(s/def :editor.extensions.validation.command/type #{"resource" "outline"})
(s/def :editor.extensions.validation.command/cardinality #{"one" "many"})
(s/def :editor.extensions.validation.command/selection
  (s/keys :req-un [:editor.extensions.validation.command/type
                   :editor.extensions.validation.command/cardinality]))
(s/def :editor.extensions.validation.command/query
  (s/keys :opt-un [:editor.extensions.validation.command/selection]))
(s/def :editor.extensions.validation.command/active lua-fn?)
(s/def :editor.extensions.validation.command/run lua-fn?)
(s/def ::command (s/keys :req-un [:editor.extensions.validation.command/label
                                  :editor.extensions.validation.command/locations]
                         :opt-un [:editor.extensions.validation.command/query
                                  :editor.extensions.validation.command/active
                                  :editor.extensions.validation.command/run]))
(s/def ::commands (s/coll-of ::command))

;; language servers

(s/def :editor.extensions.validation.language-server/languages (s/coll-of string? :min-count 1 :distinct true))
(s/def :editor.extensions.validation.language-server/command (s/coll-of string? :min-count 1))
(s/def :editor.extensions.validation.language-server/pattern string?)
(s/def :editor.extensions.validation.language-server/watched_file
  (s/keys :req-un [:editor.extensions.validation.language-server/pattern]))
(s/def :editor.extensions.validation.language-server/watched_files
  (s/coll-of :editor.extensions.validation.language-server/watched_file :min-count 1))
(s/def ::language-server
  (s/keys :req-un [:editor.extensions.validation.language-server/languages
                   :editor.extensions.validation.language-server/command]
          :opt-un [:editor.extensions.validation.language-server/watched_files]))
(s/def ::language-servers (s/coll-of ::language-server))

(defn- ->lua-string
  "Convert Clojure data structure to a string representing equivalent Lua data structure"
  [x]
  (cond
    (keyword? x)
    (pr-str (name x))

    (instance? Map x)
    (str "{"
         (->> x
              (map (fn [[k v]]
                     (let [k-str (if (and (or (string? k)
                                              (keyword? k))
                                          (re-matches #"^[a-zA-Z_][[a-zA-Z_0-9]]*$" (name k)))
                                   (name k)
                                   (str "[" (->lua-string k) "]"))]
                       (str k-str " = " (->lua-string v)))))
              (string/join ", "))
         "}")

    (instance? List x)
    (str "{" (string/join ", " (map ->lua-string x)) "}")

    (lua-fn? x)
    (str "<function>")

    :else
    (pr-str x)))

(defn- spec-pred->reason [pred]
  (or (cond
        (symbol? pred)
        (let [unqualified (symbol (name pred))]
          (case unqualified
            (coll? vector?) "is not an array"
            int? "is not an integer"
            number? "is not a number"
            string? "is not a string"
            boolean? "is not a boolean"
            lua-fn? "is not a function"
            action-spec (str "needs \"action\" key to be "
                             (->> (.getMethodTable ^MultiFn action-spec)
                                  keys
                                  sort
                                  (map #(str \" % \"))
                                  (util/join-words ", " " or ")))
            distinct? "should not have repeated elements"
            node-id? "is not a node id"
            resource-path? "is not a resource path"
            map? "is not a table"
            nil))

        (set? pred)
        (str "is not " (->> pred
                            sort
                            (map #(str \" % \"))
                            (util/join-words ", " " or ")))

        (coll? pred)
        (cond
          (= '(= (count %)) (take 2 pred))
          (str "needs to have " (last pred) " element" (when (< 1 (last pred)) "s"))

          (= '(contains? %) (take 2 pred))
          (str "needs " (->lua-string (last pred)) " key")

          (and (= '<= (first pred)) (number? (second pred)))
          (str "needs at least " (second pred) " element" (when (< 1 (second pred)) "s"))))
      (pr-str pred)))

(defn ensure [spec value]
  (if (s/valid? spec value)
    value
    (throw (LuaError. (->> (s/explain-data spec value)
                           ::s/problems
                           (map #(str (->lua-string (:val %)) " " (spec-pred->reason (s/abbrev (:pred %)))))
                           (str/join "\n"))))))