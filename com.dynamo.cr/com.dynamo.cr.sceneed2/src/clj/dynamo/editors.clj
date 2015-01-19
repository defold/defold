(ns dynamo.editors
  "This namespace provides the built in editor parts, along with support for building your own editors."
  (:require [clojure.core.async :refer [chan dropping-buffer]]
            [internal.ui.editors :as ie]
            [internal.ui.handlers :refer [active-editor]])
  (:import  [internal.ui GenericEditor]
            [org.eclipse.core.commands ExecutionEvent]))

(set! *warn-on-reflection* true)

(defn event->active-editor
  "Returns the Clojure implementation of the active editor from the event's application context."
  [^ExecutionEvent evt]
  (let [editor (active-editor (.getApplicationContext evt))]
    (if (instance? GenericEditor editor)
      (.getBehavior ^GenericEditor editor))))

(defn open-part
  "Creates an Eclipse view for the given behavior. The
behavior must support dynamo.types/MessageTarget.

Allowed options are:

:label - the visible label for the part
:closeable - if true, the part can be closed (defaults to true)
:id - the part ID by which Eclipse will know it"
  [behavior & {:as opts}]
  (ie/open-part behavior opts))
