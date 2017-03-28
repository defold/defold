(ns editor.import
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.client :as client]
            [editor.dialogs :as dialogs]
            [editor.git :as git]
            [editor.login :as login]
            [editor.prefs :as prefs]
            [editor.ui :as ui])
  (:import [com.dynamo.cr.protocol.proto Protocol$ProjectInfoList Protocol$ProjectInfo]
           [javafx.scene Parent Scene]
           [org.eclipse.jgit.api Git]))

(set! *warn-on-reflection* true)

(defn- clone-repo [prefs url directory progress-monitor]
  (doto (Git/cloneRepository)
      (.setCredentialsProvider (git/credentials prefs))
      (.setProgressMonitor progress-monitor)
      (.setURI url)
      (.setDirectory (io/file directory))
      (.call)))

(defn- clone-project [prefs dialog controls]
  (let [project (first (ui/selection (:projects controls)))
        directory (format "%s/%s" (ui/text (:location controls)) (:name project))]
    (clone-repo prefs (:repository-url project) directory (git/make-clone-monitor (dialogs/progress-bar dialog)))
    (dialogs/return! dialog (format "%s/game.project" directory))
    (ui/run-later (dialogs/close! dialog))))

(defn- ready? [controls]
  (and (seq (ui/selection (:projects controls)))
       (pos? (count (ui/text (:location controls))))))

(defn- pick-location [prefs root controls]
  (when-let [dir (ui/choose-directory "Select a root directory for the project" nil (ui/window (ui/scene root)))]
    (prefs/set-prefs prefs "import-location" dir)
    (ui/text! (:location controls) dir)))

(defn- sort-projects [project-info-list]
  (sort (fn [p1 p2] (.compareToIgnoreCase ^String (:name p1) ^String (:name p2))) (:projects project-info-list)))

(defn- fetch-projects [prefs controls]
  (let [client (client/make-client prefs)]
    (let [project-info-list (client/rget client "/projects/-1" Protocol$ProjectInfoList)]
      (ui/items! (:projects controls) (sort-projects project-info-list)))))

(defn open-import-dialog [prefs]
  (login/login prefs)
  (let [client (client/make-client prefs)
        dialog (dialogs/make-task-dialog "import.fxml"
                                         {:title "Import Project"
                                          :ok-label "Import"})
        ^Parent root (dialogs/dialog-root dialog)
        controls (ui/collect-controls root ["projects" "location" "location-picker"])
        default-location (prefs/get-prefs prefs "import-location" (System/getProperty "user.home"))]
    (ui/text! (:location controls) default-location)
    (ui/cell-factory! (:projects controls) (fn [p] {:text (:name p)}))
    (ui/on-action! (:location-picker controls) (fn [_] (pick-location prefs root controls)))
    (dialogs/task! dialog (fn [] (fetch-projects prefs controls)))
    (dialogs/show! dialog {:on-ok (fn [] (dialogs/task! dialog (fn [] (clone-project prefs dialog controls))))
                           :ready? (fn [] (ready? controls))})))
