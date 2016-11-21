(ns editor.changes-view
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.diff-view :as diff-view]
            [editor.git :as git]
            [editor.handler :as handler]
            [editor.ui :as ui]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [editor.sync :as sync]
            [service.log :as log])
  (:import [javafx.scene Parent]
           [javafx.scene.control SelectionMode ListView]
           [org.eclipse.jgit.api Git]))

(set! *warn-on-reflection* true)

(defn- refresh-list-view! [git list-view]
  (when git
    (ui/items! list-view (git/unified-status git))))

(defn refresh! [changes-view]
  (let [git (g/node-value changes-view :git)
        list-view (g/node-value changes-view :list-view)]
    (refresh-list-view! git list-view)))

(def ^:const status-icons
  {:add    "icons/32/Icons_M_07_plus.png"
   :modify "icons/32/Icons_S_06_arrowup.png"
   :delete "icons/32/Icons_M_06_trash.png"
   :rename "icons/32/Icons_S_08_arrow-d-right.png"})

(def ^:const status-styles
  {:add    #{"added-file"}
   :modify #{"modified-file"}
   :delete #{"deleted-file"}
   :rename #{"renamed-file"}})

(defn- status-render [status]
  {:text (format "%s" (or (:new-path status)
                          (:old-path status)))
   :icon (get status-icons (:change-type status))
   :style (get status-styles (:change-type status) #{})})

(ui/extend-menu ::changes-menu nil
                [{:label "Diff"
                  :icon "icons/32/Icons_S_06_arrowup.png"
                  :command :diff}
                 {:label "Revert"
                  :icon "icons/32/Icons_S_02_Reset.png"
                  :command :revert}
                 {:label :separator}
                 {:label "Open"
                  :icon "icons/32/Icons_S_14_linkarrow.png"
                  :command :open}
                 {:label "Open As"
                  :icon "icons/32/Icons_S_14_linkarrow.png"
                  :command :open-as}
                 {:label "Show in Desktop"
                  :icon "icons/32/Icons_S_14_linkarrow.png"
                  :command :show-in-desktop}])

(handler/defhandler :revert :changes-view
  (enabled? [selection]
            (pos? (count selection)))
  (run [selection git list-view workspace]
       (git/revert git (mapv (fn [status] (or (:new-path status) (:old-path status))) selection))
       (workspace/resource-sync! workspace)))

(handler/defhandler :diff :changes-view
  (enabled? [selection]
            (git/selection-diffable? selection))
  (run [selection ^Git git list-view]
       (let [{:keys [new new-path old old-path]} (git/selection-diff-data git selection)]
         (diff-view/make-diff-viewer old-path old new-path new))))

(handler/defhandler :synchronize :global
  (enabled? [changes-view]
            (g/node-value changes-view :git))
  (run [changes-view workspace]
    (let [git   (g/node-value changes-view :git)
          prefs (g/node-value changes-view :prefs)]
      (sync/open-sync-dialog (sync/begin-flow! git prefs))
      (workspace/resource-sync! workspace))))

(ui/extend-menu ::menubar :editor.app-view/open
                [{:label "Synchronize..."
                  :id ::synchronize
                  :acc "Shortcut+U"
                  :command :synchronize}])

(g/defnode ChangesView
  (inherits core/Scope)
  (property list-view g/Any)
  (property git g/Any)
  (property prefs g/Any))

(defn- status->resource [workspace status]
  (workspace/resolve-workspace-resource workspace (format "/%s" (or (:new-path status)
                                                                    (:old-path status)))))

(defn make-changes-view [view-graph workspace prefs ^Parent parent]
  (let [^ListView list-view (.lookup parent "#changes")
        diff-button         (.lookup parent "#changes-diff")
        revert-button       (.lookup parent "#changes-revert")
        git                 (try (Git/open (io/file (g/node-value workspace :root)))
                                 (catch Exception _))
        view-id             (g/make-node! view-graph ChangesView :list-view list-view :git git :prefs prefs)]
    ; TODO: try/catch to protect against project without git setup
    ; Show warning/error etc?
    (try
      (.setSelectionMode (.getSelectionModel list-view) SelectionMode/MULTIPLE)
      ; TODO: Should we really include both git and list-view in the context. Have to think about this
      (ui/context! parent :changes-view {:git git :list-view list-view :workspace workspace} (ui/->selection-provider list-view) {}
                   {resource/Resource (fn [status] (status->resource workspace status))})
      (ui/register-context-menu list-view ::changes-menu)
      (ui/cell-factory! list-view status-render)
      (ui/bind-action! diff-button :diff)
      (ui/bind-action! revert-button :revert)
      (ui/disable! diff-button true)
      (ui/disable! revert-button true)
      (ui/bind-double-click! list-view :open)
      (refresh-list-view! git list-view)
      (ui/observe-selection list-view
                            (fn [_ _]
                              (ui/refresh-bound-action-enabled! diff-button)
                              (ui/refresh-bound-action-enabled! revert-button)))
      view-id
      (catch Exception e
        (log/error :exception e)))))
