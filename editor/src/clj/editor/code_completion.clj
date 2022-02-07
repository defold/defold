;; Copyright 2020 The Defold Foundation
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

(ns editor.code-completion
  (:require [clojure.set :as set]
            [clojure.string :as string]
            [editor.lua :as lua]
            [internal.util :as util]))

(defn replace-alias [s alias]
  (string/replace s #".*\." (str alias ".")))

(defn make-completions [resource-completion include-locals? namespace-alias]
  (let [{:keys [vars local-vars functions local-functions]} resource-completion
        var-info (if include-locals? (set/union vars local-vars) vars)
        vars (map (fn [v] (lua/create-code-hint :variable v))
                  var-info)
        fn-info (if include-locals? (merge functions local-functions) functions)
        fns  (map (fn [[fname {:keys [params]}]]
                    (let [n (if namespace-alias (replace-alias fname namespace-alias) fname)
                          display-string (str n "(" (string/join ", " params)  ")")]
                      (lua/create-code-hint :function n display-string display-string "" {:select params})))
                  fn-info)]
    (set (concat vars fns))))

(defn make-local-completions
  [completion-info]
  {"" (make-completions completion-info true nil)})

(defn make-required-completions
  [requires required-completion-infos]
  (let [required-completion-info-by-module (into {} (map (juxt :module identity)) required-completion-infos)]
    (reduce (fn [ret [alias module]]
              (let [completion-info (required-completion-info-by-module module)
                    completions (make-completions completion-info false alias)
                    module-name (or alias "")]
                (merge-with set/union
                            ret
                            {module-name completions}
                            (when alias {"" #{(lua/create-code-hint :namespace alias)}}))))
            {}
            requires)))

(defn combine-completions
  [local-completion-info required-completion-infos script-intelligence-completions]
  (merge-with (fn [dest new]
                (let [taken-display-strings (into #{} (map :display-string) dest)]
                  (into dest
                        (comp (remove #(contains? taken-display-strings (:display-string %)))
                              (util/distinct-by :display-string))
                        new)))
              lua/defold-docs
              lua/lua-std-libs-docs
              script-intelligence-completions
              (make-local-completions local-completion-info)
              (make-required-completions (:requires local-completion-info) required-completion-infos)))
