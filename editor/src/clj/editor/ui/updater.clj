(ns editor.ui.updater
  (:require [editor.updater :as updater]
            [editor.ui :as ui])
  (:import [javafx.stage Stage WindowEvent]
           [javafx.animation AnimationTimer]))

(defn- update-visibility! [node updater]
  (let [has-update? (updater/has-update? updater)]
    (ui/visible! node has-update?)
    has-update?))

(defn- make-tick-fn [node updater]
  (fn [^AnimationTimer timer _]
    (when (update-visibility! node updater)
      (.stop timer))))

(defn install-check-timer! [^Stage stage node updater]
  (when-not (update-visibility! node updater)
    (let [timer (ui/->timer 1 "has-update-check" (make-tick-fn node updater))]
      (doto stage
        (.addEventHandler WindowEvent/WINDOW_SHOWN
                          (ui/event-handler event
                            (ui/timer-start! timer)))
        (.addEventHandler WindowEvent/WINDOW_HIDING
                          (ui/event-handler event
                            (ui/timer-stop! timer)))))))