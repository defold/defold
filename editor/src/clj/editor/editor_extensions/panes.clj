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

(ns editor.editor-extensions.panes
  (:require [cljfx.api :as fx]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.commands :as commands]
            [editor.editor-extensions.error-handling :as error-handling]
            [editor.editor-extensions.prefs-docs :as prefs-docs]
            [editor.editor-extensions.runtime :as rt]
            [editor.editor-extensions.ui-components :as ui-components]
            [editor.editor-extensions.ui-docs :as ui-docs]
            [editor.fxui :as fxui]
            [editor.handler :as handler]
            [editor.ui :as ui]
            [util.coll :as coll]))

(def ^:private options-coercer
  (coerce/hash-map
    :req {:id prefs-docs/serializable-keyword-coercer
          :title ui-docs/string-or-message-pattern-coercer
          :location (coerce/enum :left-top :left-bottom :right-top :right-bottom :bottom)
          :render coerce/function}
    :opt {:query (coerce/hash-map :opt commands/context-query-clauses :extra-keys false)}
    :extra-keys false))

(def ext-pane
  (rt/lua-fn pane [{:keys [rt]} options]
    (-> (rt/->clj rt options-coercer options)
        (with-meta {:type :pane})
        (rt/wrap-userdata "editor.pane(...)"))))

(def definitions-coercer
  (coerce/vector-of (coerce/wrap-with-pred coerce/userdata #(= :pane (:type (meta %))) "is not a pane")))

(defn- render-content [{:keys [rt render props]}]
  (rt/->clj rt ui-docs/component-coercer (rt/invoke-immediate-1 rt render (rt/->lua props))))

(defn- render-error [{:keys [rt label exception caught]}]
  (when caught
    (error-handling/display-script-error! rt label exception))
  {:fx/type fx.label/lifecycle :text (ex-message exception) :wrap-text true})

(ui/defc ^:private pane-content
  {:compose [{:fx/type fx/ext-recreate-on-key-changed :key (:rt props)}
             {:fx/type ui-components/root-view
              :localization (:localization props)
              :workspace (:workspace props)
              :owner-window (:owner-window props)}]}
  [{:keys [rt render props error-label]}]
  {:fx/type fx.stack-pane/lifecycle
   :alignment :top-left
   :children [{:fx/type fxui/ext-error-boundary
               :catch {:fx/type render-error :rt rt :label error-label}
               :desc {:fx/type render-content :rt rt :render render :props props}}]})

(defn compile-pane [project workspace localization rt path {:keys [id location title query render]}]
  (let [query-fn
        ((commands/compile-query query project)
         (fn describe-content [{:keys [owner-window]} props]
           {:fx/type pane-content
            :localization localization
            :workspace workspace
            :owner-window owner-window
            :rt rt
            :render render
            :props props
            :error-label (str "Rendering pane " id " in " path)}))]
    {:id id
     :location location
     :title title
     :content (fn resolve-content [{:keys [contexts] :as env}]
                (coll/some
                  (fn try-context [{:keys [adapters] context-env :env}]
                    (binding [handler/*adapters* adapters]
                      (query-fn (merge context-env (dissoc env :contexts)))))
                  contexts))}))
