(ns editor.changes-view
  (:require [clojure.java.io :as io]
            [editor.core :as core]
            [editor.git :as git]
            [editor.handler :as handler]
            [editor.ui :as ui]
            [dynamo.graph :as g]
            [dynamo.types :as t])
  (:import [javafx.scene Parent]
           [javafx.scene.control SelectionMode]
           [org.eclipse.jgit.api Git]
           [org.eclipse.jgit.storage.file FileRepositoryBuilder]))

(def short-status {:add "A" :modify "M" :delete "D" :rename "R"})

(defn refresh! [git list-view]
  (ui/items! list-view (git/unified-status git)))

(defn- status-render [status]
  {:text (format "[%s]%s" ((:change-type status) short-status) (or (:new-path status) (:old-path status)))})

(ui/extend-menu ::changes-menu nil
                [{:label "Revert"
                  :icon "icons/site_backup_and_restore.png"
                  :command :revert}])

(handler/defhandler :revert
  (enabled? [selection] (pos? (count selection)))
  (run [selection git list-view]
       (doseq [status selection]
         (git/revert git [(or (:new-path status) (:old-path status))]))
       (refresh! git list-view)))

(g/defnode ChangesView
  (inherits core/Scope)
  (property git t/Any)
  (on :create
      (let [^Parent parent (:parent event)
            refresh (.lookup parent "#changes-refresh")
            list-view (.lookup parent "#changes")]
        (.setSelectionMode (.getSelectionModel list-view) SelectionMode/MULTIPLE)
        ; TODO: Should we really include both git and list-view in the context. Have to think about this
        (ui/register-context-menu list-view {:git (:git event) :list-view list-view} list-view ::changes-menu)
        (ui/cell-factory! list-view status-render)
        (ui/on-action! refresh (fn [_] (refresh! (:git event) list-view)))
        (refresh! (:git event) list-view)))

  t/IDisposable
  (dispose [this]))

(defn make-changes-view [view-graph workspace parent]
  (let [view      (g/make-node! view-graph ChangesView :parent-view parent)
        self-ref  (g/node-id view)]
    (g/dispatch-message (g/now) view :create :parent parent :git (Git/open (io/file (:root workspace))))
    (g/refresh view)))
