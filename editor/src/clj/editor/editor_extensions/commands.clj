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

(ns editor.editor-extensions.commands
  (:require [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.editor-extensions.actions :as actions]
            [editor.editor-extensions.error-handling :as error-handling]
            [editor.editor-extensions.runtime :as rt]
            [editor.future :as future]
            [editor.handler :as handler]
            [editor.lsp.async :as lsp.async]
            [editor.outline :as outline]
            [editor.resource :as resource]))

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
  (if (= "one" (:cardinality q))
    (when (= 1 (count selection))
      (first selection))
    selection))

(defn- node-ids->lua-selection [selection q]
  (ensure-selection-cardinality (mapv rt/wrap-userdata selection) q))

(defmethod gen-selection-query "resource" [q acc project]
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
                                          (->> (keep #(project/get-resource-node project % evaluation-context)))
                                          (node-ids->lua-selection q)))]
                 (when-not (:evaluation-context env)
                   (g/update-cache-from-evaluation-context! evaluation-context))
                 (cont assoc :selection res)))))

(defmethod gen-selection-query "outline" [q acc _]
  (gen-query acc [env cont]
             (when-let [res (some-> (:selection env)
                                    (handler/adapt-every outline/OutlineNode)
                                    (node-ids->lua-selection q))]
               (cont assoc :selection res))))

(defn- compile-query [q project]
  (reduce-kv
    (fn [acc k v]
      (case k
        :selection (gen-selection-query v acc project)
        acc))
    (fn [lua-fn]
      (fn [env]
        (lua-fn env {})))
    q))

(defn command->dynamic-handler [{:keys [label query active run locations]} path project state]
  (let [{:keys [rt display-output!]} state
        lua-fn->env-fn (compile-query query project)
        contexts (into #{}
                       (map {"Assets" :asset-browser
                             "Outline" :outline
                             "Edit" :global
                             "View" :global})
                       locations)
        locations (into #{}
                        (map {"Assets" :editor.asset-browser/context-menu-end
                              "Outline" :editor.outline-view/context-menu-end
                              "Edit" :editor.app-view/edit-end
                              "View" :editor.app-view/view-end})
                        locations)]
    {:context-definition contexts
     :menu-item {:label label}
     :locations locations
     :fns (cond-> {}
                  active
                  (assoc :active?
                         (lua-fn->env-fn
                           (fn [env opts]
                             (error-handling/try-with-extension-exceptions
                               :display-output! display-output!
                               :label (str label "'s \"active\" in " path)
                               :catch false
                               (rt/->clj rt (rt/invoke-immediate (:rt state) active (rt/->lua opts) (:evaluation-context env)))))))

                  (and (not active) query)
                  (assoc :active? (lua-fn->env-fn (constantly true)))

                  run
                  (assoc :run
                         (lua-fn->env-fn
                           (fn [_ opts]
                             (let [error-label (str label "'s \"run\" in " path)]
                               (-> (rt/invoke-suspending rt run (rt/->lua opts))
                                   (future/then
                                     (fn [lua-result]
                                       (when-let [actions (rt/->clj rt lua-result)]
                                         (lsp.async/with-auto-evaluation-context evaluation-context
                                           (actions/perform! actions project state evaluation-context)))))
                                   (future/catch #(error-handling/display-script-error! display-output! error-label %))))))))}))
