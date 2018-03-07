(ns editor.welcome
  (:require [editor.ui :as ui])
  (:import (javafx.scene Node Scene Parent)
           (javafx.scene.control ToggleGroup)))

(defn- user-data [^Node node]
  (.getUserData node))

(def ^:private pane-button->pane (comp ui/load-fxml (partial str "welcome/") user-data))

(defn open-welcome-dialog! []
  (let [root (ui/load-fxml "welcome/welcome-dialog.fxml")
        stage (ui/make-dialog-stage)
        scene (Scene. root)
        left-pane ^Parent (.lookup root "#left-pane")
        pane-buttons-toggle-group (ToggleGroup.)
        pane-buttons (.lookupAll left-pane ".pane-button")
        panes-by-pane-button (into {}
                                   (map (juxt identity pane-button->pane))
                                   pane-buttons)]

    ;; TEMP HACK, REMOVE!
    ;; Reload stylesheets.
    (let [styles (vec (.getStylesheets root))]
      (.forget (com.sun.javafx.css.StyleManager/getInstance) scene)
      (.setAll (.getStylesheets root) ^java.util.Collection styles))

    ;; Configure the pane selection buttons in the left panel to toggle between panes.
    (doseq [pane-button pane-buttons]
      (.setToggleGroup pane-button pane-buttons-toggle-group))

    (ui/observe (.selectedToggleProperty pane-buttons-toggle-group)
                (fn [_ _ selected-pane-button]
                  (let [pane (panes-by-pane-button selected-pane-button)]
                    (ui/children! root [left-pane pane]))))

    ;; Select the topmost pane button.
    (when-some [first-pane-button (second pane-buttons)]
      (.selectToggle pane-buttons-toggle-group first-pane-button))

    ;; Show the dialog.
    (.setScene stage scene)
    (ui/show! stage)))
