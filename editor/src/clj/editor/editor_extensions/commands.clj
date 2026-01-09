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

(ns editor.editor-extensions.commands
  (:require [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.editor-extensions.actions :as actions]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.error-handling :as error-handling]
            [editor.editor-extensions.localization :as ext.localization]
            [editor.editor-extensions.prefs-docs :as prefs-docs]
            [editor.editor-extensions.runtime :as rt]
            [editor.editor-extensions.ui-docs :as ui-docs]
            [editor.future :as future]
            [editor.handler :as handler]
            [editor.lsp.async :as lsp.async]
            [editor.outline :as outline]
            [editor.resource :as resource]
            [editor.scene :as scene]))

(set! *warn-on-reflection* true)

(defn- continue [acc env lua-fn f & args]
  (let [new-lua-fn (fn [env m]
                     (lua-fn env (apply f m args)))]
    ((acc new-lua-fn) env)))

(defmacro gen-query [acc-sym [env-sym cont-sym] & body-expr]
  `(fn [lua-fn#]
     (fn [~env-sym]
       (let [~cont-sym (partial continue ~acc-sym ~env-sym lua-fn#)]
         ~@body-expr))))

(defmulti gen-selection-query (fn [q _acc _project]
                                (:type q)))

(defn- ensure-selection-cardinality [selection q]
  (if (= :one (:cardinality q))
    (when (= 1 (count selection))
      (first selection))
    selection))

(defn- node-ids->lua-selection [selection q]
  (ensure-selection-cardinality (mapv rt/wrap-userdata selection) q))

(defmethod gen-selection-query :resource [q acc project]
  (gen-query acc [env cont]
             (let [evaluation-context (or (:evaluation-context env) (g/make-evaluation-context))
                   selection (:selection env)]
               (when-let [res (or (some-> selection
                                          (handler/adapt-every
                                            resource/ResourceNode
                                            #(-> %
                                                 (g/node-value :resource evaluation-context)
                                                 resource/proj-path
                                                 some?))
                                          (node-ids->lua-selection q))
                                  (some-> selection
                                          (handler/adapt-every resource/Resource)
                                          (->> (into
                                                 []
                                                 (keep
                                                   (fn [resource]
                                                     (if-let [node-id (project/get-resource-node project resource evaluation-context)]
                                                       (rt/wrap-userdata node-id)
                                                       (resource/proj-path resource))))))
                                          (ensure-selection-cardinality q)))]
                 (when-not (:evaluation-context env)
                   (g/update-cache-from-evaluation-context! evaluation-context))
                 (cont assoc :selection res)))))

(defmethod gen-selection-query :outline [q acc _]
  (gen-query acc [env cont]
             (when-let [res (some-> (:selection env)
                                    (handler/adapt-every outline/OutlineNode)
                                    (node-ids->lua-selection q))]
               (cont assoc :selection res))))

(defmethod gen-selection-query :scene [q acc _]
  (gen-query acc [env cont]
             (when-let [res (some-> (:selection env)
                                    (handler/adapt-every scene/SceneNode)
                                    (node-ids->lua-selection q))]
               (cont assoc :selection res))))

(defn- compile-query [q project]
  (reduce-kv
    (fn [acc k v]
      (case k
        :selection (gen-selection-query v acc project)
        :argument (gen-query acc [env cont] (cont assoc :argument (:user-data env)))
        acc))
    (fn [lua-fn]
      (fn [env]
        (lua-fn env {})))
    q))

(def ^:private command-definition-coercer
  (coerce/hash-map
    :req {:label ui-docs/string-or-message-pattern-coercer
          :locations (coerce/vector-of
                       (coerce/enum "Assets" "Bundle" "Debug" "Edit" "Outline" "Project" "Scene" "View" "Help")
                       :distinct true
                       :min-count 1)}
    :opt {:query (coerce/hash-map
                   :opt {:selection (coerce/hash-map
                                      :req {:type (coerce/enum :resource :outline :scene)
                                            :cardinality (coerce/enum :one :many)})
                         :argument (coerce/const true)})
          :id prefs-docs/serializable-keyword-coercer
          :active coerce/function
          :run coerce/function}))

(def command-coercer
  (coerce/one-of
    (coerce/wrap-with-pred coerce/userdata #(= :command (:type (meta %))) "is not a command")
    command-definition-coercer))

(def ^:private ext-command-args-coercer
  (coerce/regex :command command-definition-coercer))

(def ext-command-fn
  (rt/varargs-lua-fn ext-command [{:keys [rt]} varargs]
    (let [{:keys [command]} (rt/->clj rt ext-command-args-coercer varargs)]
      (-> command
          (with-meta {:type :command})
          (rt/wrap-userdata "editor.command(...)")))))

(defn command->dynamic-handler [{:keys [label query active id run locations]} path project state]
  (let [{:keys [rt display-output!]} state
        lua-fn->env-fn (compile-query query project)
        contexts (into #{}
                       (map {"Assets" :asset-browser
                             "Bundle" :global
                             "Debug" :global
                             "Edit" :global
                             "Outline" :outline
                             "Project" :global
                             "Scene" :global
                             "View" :global
                             "Help" :global})
                       locations)
        locations (into #{}
                        (map {"Assets" :editor.asset-browser/context-menu-end
                              "Bundle" :editor.bundle/menu
                              "Debug" :editor.debug-view/debug-end
                              "Edit" :editor.app-view/edit-end
                              "Outline" :editor.outline-view/context-menu-end
                              "Project" ::project/project-end
                              "Scene" :editor.scene-selection/context-menu-end
                              "View" :editor.app-view/view-end
                              "Help" :editor.app-view/help-end})
                        locations)]
    (cond-> {:contexts contexts
             :label label
             :locations locations}

            id
            (assoc :command id)

            active
            (assoc :active?
                   (lua-fn->env-fn
                     (fn [env opts]
                       (error-handling/try-with-extension-exceptions
                         :display-output! display-output!
                         :label (str label "'s \"active\" in " path)
                         :catch false
                         (rt/->clj rt coerce/to-boolean (rt/invoke-immediate-1 (:rt state) active (rt/->lua opts) (:evaluation-context env)))))))

            (and (not active) query)
            (assoc :active? (lua-fn->env-fn (constantly true)))

            run
            (assoc :run
                   (lua-fn->env-fn
                     (fn [_ opts]
                       (let [error-label (str label "'s \"run\" in " path)]
                         (-> (rt/invoke-suspending-1 rt run (rt/->lua opts))
                             (future/then
                               (fn [lua-result]
                                 (when-not (rt/coerces-to? rt coerce/null lua-result)
                                   (lsp.async/with-auto-evaluation-context evaluation-context
                                     (actions/perform! lua-result project state evaluation-context)))))
                             (future/catch #(error-handling/display-script-error! display-output! error-label %))))))))))
