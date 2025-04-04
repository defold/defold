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

(ns editor.changes-view
  (:require [dynamo.graph :as g]
            [editor.core :as core]
            [editor.dialogs :as dialogs]
            [editor.diff-view :as diff-view]
            [editor.disk-availability :as disk-availability]
            [editor.error-reporting :as error-reporting]
            [editor.git :as git]
            [editor.handler :as handler]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.vcs-status :as vcs-status]
            [editor.workspace :as workspace]
            [service.log :as log])
  (:import [java.io File]
           [javafx.beans.value ChangeListener]
           [javafx.scene Parent]
           [javafx.scene.control ListView SelectionMode]
           [org.eclipse.jgit.api Git]))

(set! *warn-on-reflection* true)

(defn- refresh-list-view! [list-view unified-status]
  (ui/items! list-view unified-status))

(defn refresh! [changes-view]
  (let [list-view (g/node-value changes-view :list-view)
        progress-overlay (g/node-value changes-view :progress-overlay)
        git (g/node-value changes-view :git)
        refresh-pending (ui/user-data list-view :refresh-pending)
        schedule-refresh (ref nil)]
    (when git
      (dosync
        (ref-set schedule-refresh (not @refresh-pending))
        (ref-set refresh-pending true))
      (when @schedule-refresh
        (ui/select! list-view nil)
        (ui/visible! progress-overlay true)
        (future
          (try
            (dosync (ref-set refresh-pending false))
            (let [unified-status (git/unified-status git)]
              (ui/run-later
                (try
                  (refresh-list-view! list-view unified-status)
                  (finally
                    (ui/visible! progress-overlay false)))))
            (catch Throwable error
              (ui/run-later
                (ui/visible! progress-overlay false))
              (error-reporting/report-exception! error))))))))

(handler/register-menu! ::changes-menu
  [{:label "Open"
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :open}
   {:label "Open As"
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :open-as}
   {:label :separator}
   {:label "Copy Project Path"
    :command :copy-project-path}
   {:label "Copy Full Path"
    :command :copy-full-path}
   {:label "Copy Require Path"
    :command :copy-require-path}
   {:label :separator}
   {:label "Show in Asset Browser"
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :show-in-asset-browser}
   {:label "Show in Desktop"
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :show-in-desktop}
   {:label "Referencing Files..."
    :command :referencing-files}
   {:label "Dependencies..."
    :command :dependencies}
   {:label "Show Overrides"
    :command :show-overrides}
   {:label :separator}
   {:label "View Diff"
    :icon "icons/32/Icons_S_06_arrowup.png"
    :command :diff}
   {:label "Revert"
    :icon "icons/32/Icons_S_02_Reset.png"
    :command :revert}])

(defn- path->file
  ^File [workspace ^String path]
  (File. (workspace/project-directory workspace) path))

(handler/defhandler :revert :changes-view
  (enabled? [selection]
            (and (disk-availability/available?)
                 (pos? (count selection))))
  (run [async-reload! selection git changes-view workspace]
    (when (dialogs/make-confirmation-dialog
            {:title "Revert Changes?"
             :size :large
             :icon :icon/circle-question
             :header "Are you sure you want to revert changes on selected files?"
             :buttons [{:text "Cancel"
                        :cancel-button true
                        :default-button true
                        :result false}
                       {:text "Revert Changes"
                        :variant :danger
                        :result true}]})
      (let [moved-files (mapv #(vector (path->file workspace (:new-path %)) (path->file workspace (:old-path %))) (filter #(= (:change-type %) :rename) selection))]
        (git/revert git (mapv (fn [status] (or (:new-path status) (:old-path status))) selection))
        (async-reload! changes-view moved-files)))))

(handler/defhandler :diff :changes-view
  (enabled? [selection]
            (git/selection-diffable? selection))
  (run [selection ^Git git]
       (diff-view/present-diff-data (git/selection-diff-data git selection))))

(g/defnode ChangesView
  (inherits core/Scope)
  (property list-view g/Any)
  (property progress-overlay g/Any)
  (property git g/Any)
  (property prefs g/Any))

(defn- status->resource [workspace status]
  (workspace/resolve-workspace-resource workspace (format "/%s" (or (:new-path status)
                                                                    (:old-path status)))))

(defn- try-open-git
  ^Git [workspace]
  (let [repo-directory (workspace/project-directory workspace)
        git (git/try-open repo-directory)
        head-commit (some-> git .getRepository (git/get-commit "HEAD"))]
    (if (some? head-commit)
      git
      (when (some? git)
        (.close git)))))

(defn make-changes-view [view-graph workspace prefs ^Parent parent async-reload!]
  (assert (fn? async-reload!))
  (let [^ListView list-view     (.lookup parent "#changes")
        diff-button             (.lookup parent "#changes-diff")
        revert-button           (.lookup parent "#changes-revert")
        progress-overlay        (.lookup parent "#changes-progress-overlay")
        git                     (try-open-git workspace)
        view-id                 (g/make-node! view-graph ChangesView :list-view list-view :progress-overlay progress-overlay :git git :prefs prefs)
        disk-available-listener (reify ChangeListener
                                  (changed [_this _observable _old _new]
                                    (ui/refresh-bound-action-enabled! revert-button)))]
    (ui/user-data! list-view :refresh-pending (ref false))
    (.setSelectionMode (.getSelectionModel list-view) SelectionMode/MULTIPLE)
    (ui/context! parent :changes-view {:async-reload! async-reload! :changes-view view-id :workspace workspace} (ui/->selection-provider list-view)
                 {:git [:changes-view :git]}
                 {resource/Resource (fn [status] (status->resource workspace status))})
    (ui/register-context-menu list-view ::changes-menu)
    (ui/cell-factory! list-view vcs-status/render)
    (ui/bind-action! diff-button :diff)
    (ui/bind-action! revert-button :revert)
    (ui/disable! diff-button true)
    (ui/disable! revert-button true)
    (ui/visible! progress-overlay false)
    (ui/bind-double-click! list-view :open)
    ; TODO: try/catch to protect against project without git setup
    ; Show warning/error etc?
    (try
      (when git (refresh-list-view! list-view (git/unified-status git)))
      (catch Exception e
        (log/error :exception e)))
    (.addListener disk-availability/available-property disk-available-listener)
    (ui/on-closed! (.. parent getScene getWindow)
                   (fn [_]
                     (.removeListener disk-availability/available-property disk-available-listener)))
    (ui/observe-selection list-view
                          (fn [_ _]
                            (ui/refresh-bound-action-enabled! diff-button)
                            (ui/refresh-bound-action-enabled! revert-button)))
    view-id))
