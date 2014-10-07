(ns internal.ui.editors
  (:require [dynamo.project :as p]
            [dynamo.types :refer [MessageTarget]]
            [internal.system :as sys])
  (:import [org.eclipse.ui PlatformUI]
           [org.eclipse.e4.core.contexts IEclipseContext]
           [org.eclipse.e4.ui.model.application.ui.basic MBasicFactory MPart]
           [org.eclipse.e4.ui.workbench.modeling EPartService EPartService$PartState]))

(set! *warn-on-reflection* true)

(defn implementation-for
  "Factory for values that implement the Editor protocol.
   When called with an editor site and an input file, returns an
   appropriate Editor value."
  [site file]
  (let [proj (sys/project-state)]
    ((p/editor-for
       proj
       (.. file getFullPath getFileExtension))
      proj site file)))

(defn- dynamic-part
  [id label clsname]
  (doto (.createPart (MBasicFactory/INSTANCE))
    (.setElementId id)
    (.setLabel label)
    (.setCloseable true)
    (.setContributionURI (str "bundleclass://com.dynamo.cr.sceneed2/" clsname))))

(defn open-part
  [behavior]
  (assert (satisfies? MessageTarget behavior) "Behavior must support protocol dynamo.types/MessageTarget")
  (let [ctx ^IEclipseContext (.getService (PlatformUI/getWorkbench) IEclipseContext)]
    (.set ctx "behavior" behavior)
    (let [p (dynamic-part "test.view" "Testing from Clj" "internal.ui.InjectablePart")]
      (.showPart ^EPartService (.get ctx EPartService) ^MPart p EPartService$PartState/ACTIVATE)
      p)))
