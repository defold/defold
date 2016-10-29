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
           [org.eclipse.jgit.api Git]
           [org.eclipse.jgit.storage.file FileRepositoryBuilder]))

(set! *warn-on-reflection* true)

(defn refresh! [git list-view]
  (when git
    (ui/items! list-view (git/unified-status git))))

(def ^:const status-icons
  {:add    "icons/32/Icons_M_07_plus.png"
   :modify "icons/32/Icons_S_06_arrowup.png"
   :delete "icons/32/Icons_M_06_trash.png"
   :rename "icons/32/Icons_S_08_arrow-d-right.png"})

(def ^:const status-styles
  {:add    "-fx-text-fill: #00FF00;"
   :delete "-fx-text-fill: #FF0000;"
   :rename "-fx-text-fill: #0000FF;"})

(def ^:const status-default-style
  "-fx-background-color: #272b30;")

(defn- status-render [status]
  {:text (format "%s" (or (:new-path status)
                          (:old-path status)))
   :icon (get status-icons (:change-type status))
   :style (get status-styles (:change-type status) status-default-style)})

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
       (refresh! git list-view)
       (workspace/resource-sync! workspace)))

(handler/defhandler :diff :changes-view
  (enabled? [selection]
            (and (= 1 (count selection))
                 (not= :add (:change-type (first selection)))))
  (run [selection ^Git git list-view]
       (let [status (first selection)
             old-name (or (:old-path status) (:new-path status) )
             new-name (or (:new-path status) (:old-path status) )
             work-tree (.getWorkTree (.getRepository git))
             old (String. ^bytes (git/show-file git old-name))
             new (slurp (io/file work-tree new-name))]
         (diff-view/make-diff-viewer old-name old new-name new))))

(handler/defhandler :synchronize :global
  (enabled? [changes-view]
            (g/node-value changes-view :git))
  (run [changes-view]
    (let [git   (g/node-value changes-view :git)
          prefs (g/node-value changes-view :prefs)]
      (sync/open-sync-dialog (sync/begin-flow! git prefs)))))

(ui/extend-menu ::menubar :editor.app-view/open
                [{:label "Synchronize..."
                  :id ::synchronize
                  :acc "Shortcut+U"
                  :command :synchronize}])

(g/defnode ChangesView
  (inherits core/Scope)
  (property parent-view g/Any)
  (property git g/Any)
  (property prefs g/Any))

(defn- status->resource [workspace status]
  (workspace/resolve-workspace-resource workspace (format "/%s" (or (:new-path status)
                                                                    (:old-path status)))))

(defn make-changes-view [view-graph workspace prefs ^Parent parent]
  (let [git     (try (Git/open (io/file (g/node-value workspace :root)))
                     (catch Exception _))
        view-id (g/make-node! view-graph ChangesView :parent-view parent :git git :prefs prefs)
        view    (g/node-by-id view-id)]
    ; TODO: try/catch to protect against project without git setup
    ; Show warning/error etc?
    (try
      (let [refresh                      (.lookup parent "#changes-refresh")
            ^ListView list-view          (.lookup parent "#changes")]
        (.setSelectionMode (.getSelectionModel list-view) SelectionMode/MULTIPLE)
        ; TODO: Should we really include both git and list-view in the context. Have to think about this
        (ui/context! list-view :changes-view {:git git :list-view list-view :workspace workspace} list-view {}
                     {resource/Resource (fn [status] (status->resource workspace status))})
        (ui/register-context-menu list-view ::changes-menu)
        (ui/cell-factory! list-view status-render)
        (ui/on-action! refresh (fn [_] (refresh! git list-view)))
        (ui/bind-double-click! list-view :open)
        (when-not git
          (ui/disable! refresh true))
        (refresh! git list-view))
      view-id
      (catch Exception e
        (log/error :exception e)))))
