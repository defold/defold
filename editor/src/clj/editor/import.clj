(ns editor.import
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.client :as client]
            [editor.dialogs :as dialogs]
            [editor.ui.fuzzy-combo-box :as fuzzy-combo-box]
            [editor.git :as git]
            [editor.login :as login]
            [editor.prefs :as prefs]
            [editor.ui :as ui])
  (:import [com.dynamo.cr.protocol.proto Protocol$ProjectInfoList Protocol$ProjectInfo]
           [javafx.scene Node Parent Scene]
           [javafx.scene.layout Pane GridPane]
           [java.util.function UnaryOperator]
           [org.eclipse.jgit.api Git]))

(set! *warn-on-reflection* true)

(defn- clone-repo [prefs url directory progress-monitor]
  (doto (Git/cloneRepository)
      (.setCredentialsProvider (git/credentials prefs))
      (.setProgressMonitor progress-monitor)
      (.setURI url)
      (.setDirectory (io/file directory))
      (.call)))

(defn- clone-project [prefs dialog project location]
  (let [directory (format "%s/%s" location (:name project))]
    (clone-repo prefs (:repository-url project) directory (git/make-clone-monitor (dialogs/progress-bar dialog)))
    (dialogs/return! dialog (format "%s/game.project" directory))
    (ui/run-later (dialogs/close! dialog))))

(defn- pick-location [prefs root controls]
  (when-let [dir (ui/choose-directory "Select a root directory for the project" nil (ui/window (ui/scene root)))]
    (prefs/set-prefs prefs "import-location" dir)
    (ui/text! (:location controls) dir)))

(defn- sort-projects [project-info-list]
  (sort (fn [p1 p2] (.compareToIgnoreCase ^String (:name p1) ^String (:name p2))) (:projects project-info-list)))

(defn fetch-projects [prefs]
  (let [client (client/make-client prefs)]
    (let [project-info-list (client/rget client "/projects/-1" Protocol$ProjectInfoList)]
      (sort-projects project-info-list))))

(defn- ->un-op [fn1]
  (reify
    java.util.function.UnaryOperator
    (apply [this arg]
      (fn1 arg))))

(defn- replace-child! [^Pane parent ^Node replaced ^Node replacement]
  (.replaceAll (.getChildren parent) (->un-op (fn [child] (if (= child replaced) replacement child)))))

(defn open-import-dialog [prefs]
  (login/login prefs)
  (let [client (client/make-client prefs)
        projects (fetch-projects prefs)
        project-options (map (juxt identity :name) projects)
        fuzzy-project-combo (doto (fuzzy-combo-box/make project-options)
                              (GridPane/setConstraints 1 0))
        dialog (dialogs/make-task-dialog "import.fxml"
                                         {:title "Import Project"
                                          :ok-label "Import"})
        ^Parent root (dialogs/dialog-root dialog)
        controls (ui/collect-controls root ["projects" "location" "location-picker"])
        placeholder-project-combo (:projects controls)
        default-location (prefs/get-prefs prefs "import-location" (System/getProperty "user.home"))]
    (ui/text! (:location controls) default-location)
    (replace-child! ^GridPane (.getParent ^Node placeholder-project-combo) placeholder-project-combo fuzzy-project-combo)
    (fuzzy-combo-box/observe! fuzzy-project-combo (fn [_ _] (dialogs/refresh! dialog)))
    (ui/on-action! (:location-picker controls) (fn [_] (pick-location prefs root controls)))
    (dialogs/task! dialog (fn []))
    (dialogs/show! dialog {:on-ok (fn []
                                    (dialogs/task! dialog (fn [] (clone-project prefs
                                                                                dialog
                                                                                (fuzzy-combo-box/value fuzzy-project-combo)
                                                                                (ui/text (:location controls))))))
                           :ready? (fn []
                                     (and (fuzzy-combo-box/value fuzzy-project-combo)
                                          (pos? (count (ui/text (:location controls))))))})))
