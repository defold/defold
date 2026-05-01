;; Copyright 2020-2026 The Defold Foundation
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

(ns editor.script-api
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code-completion :as code-completion]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.code.script-intelligence :as si]
            [editor.defold-project :as project]
            [editor.localization :as localization]
            [editor.lua :as lua]
            [editor.resource :as resource]
            [editor.yaml :as yaml]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- bracketname?
  [x]
  (= \[ (first (:name x))))

(defn- build-param-string [params mode]
  (str "("
       (->> params
            (keep-indexed
              (fn [i {:keys [name optional] :as param}]
                (case mode
                  :display-string (if (and optional (not (bracketname? param)))
                                    (str "[" name "]")
                                    name)
                  :snippet (when (and (not optional) (not (bracketname? param)))
                             (format "${%s:%s}" (inc ^long i) name)))))
            (string/join ", "))
       ")"))

(defn- args->html [args]
  (str "<dl>"
       (->> args
            (map (fn [{:keys [name type desc parameters members]}]
                   (let [nested-args (or parameters members)]
                     (format "<dt><code>%s%s</code></dt>%s"
                             (or name "")
                             (cond
                               (string? type)
                               (format " <small>%s</small>" type)

                               (and (vector? type)
                                    (pos? (count type)))
                               (format " <small>%s</small>" (string/join "|" type))

                               :else
                               "")
                             (if (or desc nested-args)
                               (format "<dd>%s%s</dd>"
                                       (or desc "")
                                       (if nested-args
                                         (args->html nested-args)
                                         ""))
                               "")))))
            string/join)
       "</dl>"))

(defn lines->completion-info [lines]
  (letfn [(make-completions [ns-path {:keys [type name desc] :as el}]
            (case type
              "table"
              (let [child-path (conj ns-path name)]
                (into [[ns-path (code-completion/make name :type :module :doc desc)]]
                      (mapcat #(make-completions child-path %))
                      (:members el)))

              "function"
              (let [{:keys [parameters returns examples]} el]
                [[ns-path (code-completion/make
                            name
                            :type :function
                            :doc (str desc
                                      (when (pos? (count parameters))
                                        (str "\n\n**Parameters:**<br>" (args->html parameters)))
                                      (when (pos? (count returns))
                                        (str "\n\n**Returns:**<br>" (args->html returns)))
                                      (when (pos? (count examples))
                                        (str "\n\n**Examples:**<br>\n"
                                             (string/join "\n\n" (map :desc examples)))))
                            :display-string (str name (build-param-string parameters :display-string))
                            :insert (str name (build-param-string parameters :snippet)))]])

              (when name
                [[ns-path (code-completion/make name :type :variable :doc desc)]])))]
    (->> (yaml/load (data/lines-reader lines) keyword)
         (eduction (mapcat #(make-completions [] %)))
         lua/make-completion-map)))

(g/defnk produce-completions
  [parse-result]
  (g/precluding-errors parse-result parse-result))

(g/defnk produce-parse-result
  [_node-id resource lines]
  (g/package-if-error
    _node-id
    (yaml/with-error-translation _node-id :parse-result resource
      (lines->completion-info lines))))

(g/defnk produce-build-errors
  [parse-result]
  (when (g/error-package? parse-result)
    parse-result))

(g/defnode ScriptApiNode
  (inherits r/CodeEditorResourceNode)
  (output parse-result g/Any :cached produce-parse-result)
  (output build-errors g/Any produce-build-errors)
  (output completions si/ScriptCompletions produce-completions))

(defn- additional-load-fn
  [project self resource]
  (let [si (project/script-intelligence project)]
    (concat (g/connect self :completions si :lua-completions)
            (when (resource/file-resource? resource)
              ;; Only connect to the script-intelligence build errors if this is
              ;; a file resource. The assumption is that if it is a file
              ;; resource then it is being actively worked on. Otherwise, it
              ;; belongs to an external dependency and should not stop the build
              ;; on errors.
              (g/connect self :build-errors si :build-errors)))))

(defn register-resource-types
  [workspace]
  (r/register-code-resource-type workspace
    :ext "script_api"
    :label (localization/message "resource.type.script-api")
    :icon "icons/32/Icons_29-AT-Unknown.png"
    :view-types [:code :default]
    :view-opts nil
    :node-type ScriptApiNode
    :additional-load-fn additional-load-fn
    :textual? true
    :lazy-loaded true
    :language "yaml"))
