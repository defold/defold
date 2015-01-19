(ns commands.frame-object
  (:require [dynamo.editors :as ed]
            [dynamo.types :as t]
            [dynamo.ui :as ui :refer [defcommand defhandler]])
  (:import  [org.eclipse.core.commands ExecutionEvent]))

(defn frame-objects
  [^ExecutionEvent evt]
  (when-let [editor (ed/event->active-editor evt)]
    (t/process-one-event editor {:type :reframe})))

;; MENUS
;; undefine command if defined
(defcommand frame-objects-cmd
      "com.dynamo.cr.menu-items.scene"
      "com.dynamo.cr.clojure-eclipse.commands-atlas.frame-objects"
      "Frame Objects")
(defhandler frame-objects-handler frame-objects-cmd frame-objects)