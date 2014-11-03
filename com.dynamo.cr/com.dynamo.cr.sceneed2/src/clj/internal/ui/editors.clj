(ns internal.ui.editors
  (:require [dynamo.project :as p]
            [dynamo.system :as ds]
            [dynamo.types :refer [MessageTarget]]
            [internal.query :as iq]
            [internal.system :as is]
            [service.log :as log])
  (:import [org.eclipse.core.resources IFile IProject]
           [org.eclipse.ui PlatformUI]
           [org.eclipse.ui.internal.registry FileEditorMapping EditorRegistry]
           [org.eclipse.e4.core.contexts IEclipseContext]
           [org.eclipse.e4.ui.model.application.ui.basic MBasicFactory MPart]
           [org.eclipse.e4.ui.workbench.modeling EPartService EPartService$PartState]))

(set! *warn-on-reflection* true)

(defn- eclipse-project-name
  [project-node]
  (.getName (.getDescription ^IProject (:eclipse-project project-node))))

(defn- warn-multiple-projects
  [file project-nodes]
  (let [actual (first project-nodes)]
    (log/warn :multiple-projects
      (str "File " file " is referenced by " (count project-nodes) " projects. Using " (eclipse-project-name actual) " (node " (:_id actual) ")."))))

(defn- project-containing [world-ref ^IFile file]
  (let [eclipse-project (.getProject file)
        project-nodes (iq/query world-ref [[:eclipse-project eclipse-project]])]
    (case (count project-nodes)
      0   (log/error :not-project-file (str "File " file " is not part of any project"))
      1   (first project-nodes)
      (do
        (warn-multiple-projects file project-nodes)
        (first project-nodes)))))

(defn implementation-for
  "Given an editor site and input file, call the factory function associated
  with the file type (registered with `register-editor`.)"
  [site ^IFile file]
  (let [world-ref (-> @is/the-system :world :state)
        proj      (project-containing world-ref file)]
    (ds/in proj
      ((p/editor-for
         proj
         (.. file getFullPath getFileExtension))
        world-ref proj site file))))

(defn- dynamic-part
  [{:keys [id label closeable] :or {id "sceneed.view" label "Default part" closeable true}}]
  (doto (.createPart (MBasicFactory/INSTANCE))
    (.setElementId id)
    (.setLabel label)
    (.setCloseable closeable)
    (.setContributionURI (str "bundleclass://com.dynamo.cr.sceneed2/internal.ui.InjectablePart"))))

(defn open-part
  [behavior opts]
  (assert (satisfies? MessageTarget behavior) "Behavior must support protocol dynamo.types/MessageTarget")
  (let [ctx ^IEclipseContext (.getService (PlatformUI/getWorkbench) IEclipseContext)]
    (.set ctx "behavior" behavior)
    (let [p (dynamic-part opts)]
      (.showPart ^EPartService (.get ctx EPartService) ^MPart p EPartService$PartState/ACTIVATE)
      p)))