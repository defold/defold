(ns editor.code-completion
  (:require [clojure.string :as string]
            [clojure.set :as set]
            [editor.code :as code]
            [editor.defold-project :as project]
            [editor.lua :as lua]
            [editor.lua-parser :as lua-parser]
            [editor.resource :as resource]
            [dynamo.graph :as g]))

(defn replace-alias [s alias]
  (string/replace s #".*\." (str alias ".")))

(defn make-completions [resource-completion include-locals? namespace-alias]
  (let [{:keys [vars local-vars functions local-functions]} resource-completion
        var-info (if include-locals? (set/union vars local-vars) vars)
        vars (map (fn [v]  (code/create-hint v))
                  var-info)
        fn-info (if include-locals? (merge functions local-functions) functions)
        fns  (map (fn [[fname {:keys [params]}]]
                    (let [n (if namespace-alias (replace-alias fname namespace-alias) fname)
                          display-string (str n "(" (string/join "," params)  ")")]
                      (code/create-hint n display-string display-string "" {:select params})))
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
                            {module-name (make-completions completion-info false alias)}
                            (when alias {"" #{(code/create-hint alias)}}))))
            {}
            requires)))

(defn combine-completions
  [local-completion-info required-completion-infos]
  (merge-with into
              @lua/defold-docs
              @lua/lua-std-libs-docs
              (make-local-completions local-completion-info)
              (make-required-completions (:requires local-completion-info) required-completion-infos)))
