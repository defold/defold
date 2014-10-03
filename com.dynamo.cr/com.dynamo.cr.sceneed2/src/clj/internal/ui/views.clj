(ns internal.ui.views
  (:require dynamo.parts)
  (:import [org.eclipse.ui PlatformUI]
           [org.eclipse.e4.core.contexts IEclipseContext]
           [org.eclipse.e4.ui.model.application.ui.basic MBasicFactory]
           [org.eclipse.e4.ui.workbench.modeling EPartService EPartService$PartState]))

(defn- dynamic-part
  [id label clsname]
  (doto (.createPart (MBasicFactory/INSTANCE))
    (.setElementId id)
    (.setLabel label)
    (.setCloseable true)
    (.setContributionURI (str "bundleclass://com.dynamo.cr.sceneed2/" clsname))))

(defn open-part
  [behavior]
  (assert (satisfies? Part behavior) "Behavior must support protocol dynamo.parts/Part")
  (let [ctx (.getService (PlatformUI/getWorkbench) IEclipseContext)]
    (.set ctx "behavior" behavior)
    (let [p (dynamic-part "test.view" "Testing from Clj" "internal.ui.InjectablePart")]
      (.showPart (.get ctx EPartService) p EPartService$PartState/ACTIVATE))))