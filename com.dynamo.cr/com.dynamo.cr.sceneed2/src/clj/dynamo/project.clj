(ns dynamo.project
  (:require [clojure.java.io :as io]
            [plumbing.core :refer [defnk]]
            [schema.core :as s]
            [dynamo.system :as ds]
            [dynamo.file :as file]
            [dynamo.node :as n :refer [defnode Scope]]
            [dynamo.property :as dp]
            [dynamo.selection :as selection]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.util :refer :all]
            [internal.clojure :as clojure]
            [internal.query :as iq]
            [internal.ui.dialogs :as dialogs]
            [eclipse.resources :as resources]
            [service.log :as log])
  (:import [org.eclipse.core.resources IContainer IFile IProject IResource]
           [org.eclipse.core.runtime IProgressMonitor]
           [org.eclipse.ui PlatformUI IEditorSite]
           [org.eclipse.ui.internal.registry FileEditorMapping EditorRegistry]))

(set! *warn-on-reflection* true)

(defn register-filetype
  [extension default?]
  (ui/swt-safe
    (let [reg ^EditorRegistry (.getEditorRegistry (PlatformUI/getWorkbench))
          desc (.findEditor reg "com.dynamo.cr.sceneed2.scene-editor")
          mapping (doto
                    (FileEditorMapping. extension)
                    (.addEditor desc))
          all-mappings (.getFileEditorMappings reg)]
      (when default? (.setDefaultEditor mapping desc))
      (.setFileEditorMappings reg (into-array (concat (seq all-mappings) [mapping]))))))

(defn- handle
  [key filetype handler]
  (ds/update-property (ds/current-scope) :handlers assoc-in [key filetype] handler))

(defn register-loader
  [filetype loader]
  (handle :loader filetype loader))

(defn load-placeholder
  [& _]
  (ds/add (n/make-placeholder)))

(defn register-node-type
  [filetype node-type]
  (ds/update-property (ds/current-scope) :node-types assoc filetype node-type))

(defn register-editor
  [filetype editor-builder]
  (handle :editor filetype editor-builder)
  (register-filetype filetype true))

(defn register-presenter
  [property presenter]
  (ds/update-property (ds/current-scope) :presenter-registry dp/register-presenter property presenter))

(def default-handlers {:loader {"clj" clojure/on-load-code}})

(defn no-such-handler
  [key ext]
  (fn [& _] (throw (ex-info (str "No " (name key) " has been registered that can handle " ext) {}))))

(defn- editor-for [project-scope ext]
  (or
    (get-in project-scope [:handlers :editor ext])
    (no-such-handler key ext)))

(defn node?! [n kind]
  (assert (satisfies? t/Node n) (str kind " functions must return a node. Received " (type n) "."))
  n)

(defnode Placeholder
  (inherits n/ResourceNode))

(defn load-resource
  [project-node path]
  (ds/transactional
    (ds/in project-node
      (let [type          (get-in project-node [:node-types (t/extension path)] Placeholder)
            resource-node (ds/add ((:constructor type) ::filename path))]
        (when (:load (t/events resource-node))
          (ds/send-after project-node {:type :load :path path}))
        resource-node))))

(defn node-by-filename
  ([project-node path]
    (node-by-filename project-node path load-resource))
  ([project-node path factory]
    (if-let [node (first (iq/query (:world-ref project-node) [[:_id (:_id project-node)] '(input :nodes) [::filename path]]))]
      node
      (factory project-node path))))

(defnode CannedProperties
  (property rotation     s/Str  (default "twenty degrees starboard"))
  (property translation  s/Str  (default "Guten abend."))
  (property some-vector  t/Vec3 (default [1 2 3]))
  (property some-integer s/Int  (default 42)))

(defn- build-content-node
  [project-node path]
  (node-by-filename project-node path load-resource))

(defn- build-editor-node
  [project-node site path content-node]
  (let [editor-factory (editor-for project-node (t/extension path))]
    (node?! (editor-factory project-node site content-node) "Editor")))

(defn- build-selection-node
  [editor-node selected-nodes]
  (ds/in editor-node
    (let [selection-node  (ds/add (selection/make-selection))
          properties-node (ds/add (make-canned-properties :rotation "e to the i pi"))]
      (doseq [node selected-nodes]
        (ds/connect node :self selection-node :selected-nodes))
      selection-node)))

(defn make-editor
  [project-node path ^IEditorSite site]
  (let [[editor-node selection-node] (ds/transactional
                                       (ds/in project-node
                                         (let [content-node   (build-content-node project-node path)
                                               editor-node    (build-editor-node project-node site path content-node)
                                               selection-node (build-selection-node editor-node [content-node])]
                                           (when ((t/inputs editor-node) :presenter-registry)
                                             (ds/connect project-node :presenter-registry editor-node :presenter-registry))
                                           (when (and ((t/inputs editor-node) :saveable) ((t/outputs content-node) :save))
                                             (ds/connect content-node :save editor-node :saveable))
                                           (when (and ((t/inputs editor-node) :dirty) ((t/outputs content-node) :dirty))
                                             (ds/connect content-node :dirty editor-node :dirty))
                                           [editor-node selection-node])))]
    (.setSelectionProvider site selection-node)
    editor-node))

(defn- send-project-scope-message
  [graph self txn]
  (doseq [id (:nodes-added txn)]
    (ds/send-after {:_id id} {:type :project-scope :scope self})))

(defn nodes-with-extensions
  [project-node extensions]
  (let [extensions (into #{} extensions)
        pred (fn [node] (and (:filename node)
                          (some #{(t/extension (:filename node))} extensions)))]
    (into #{}
      (filter pred
        (iq/query (:world-ref project-node) [[:_id (:_id project-node)] '(input :nodes)])))))

(defn select-resources
  ([project-node extensions]
    (select-resources project-node extensions "Select resource" false))
  ([project-node extensions title]
    (select-resources project-node extensions title false))
  ([project-node extensions title multiselect?]
    (dialogs/resource-selection-dialog title multiselect? (nodes-with-extensions project-node extensions))))

; ---------------------------------------------------------------------------
; Lifecycle, Called by Eclipse
; ---------------------------------------------------------------------------
(defnode Project
  (inherits Scope)

  (property triggers           n/Triggers (default [#'n/inject-new-nodes #'send-project-scope-message]))
  (property tag                s/Keyword (default :project))
  (property eclipse-project    IProject)
  (property content-root       IContainer)
  (property branch             s/Str)
  (property presenter-registry t/Registry)

  (on :destroy
    (ds/delete self)))

(defn load-resource-nodes
  [project-node resources ^IProgressMonitor monitor]
  (ds/transactional
    (let [eclipse-project ^IProject (:eclipse-project project-node)]
      (ds/in project-node
        (doseq [resource resources]
          (let [p (file/project-path project-node resource)]
            (monitored-work monitor (str "Scanning " (t/local-name p))
              (load-resource project-node p)))))
      (monitored-work monitor (str "Compiling tools")
        project-node))))

(defn load-project
  [^IProject eclipse-project branch ^IProgressMonitor monitor]
  (ds/transactional
    (ds/add
      (make-project
        :eclipse-project eclipse-project
        :content-root (.getFolder eclipse-project "content")
        :branch branch
        :handlers default-handlers))))

(defn load-project-and-tools
  [^IProject eclipse-project branch ^IProgressMonitor monitor]
  (let [project-node    (load-project eclipse-project branch monitor)
        resources       (group-by clojure/clojure-source? (filter #(instance? IFile %) (resources/resource-seq eclipse-project)))
        clojure-sources (get resources true)
        non-sources     (get resources false)]
    (monitored-task monitor "Scanning project" (inc (count resources))
      (-> project-node
        (load-resource-nodes (get resources true) monitor)
        (load-resource-nodes (get resources false) monitor)))))

(defn open-project
  [eclipse-project branch ^IProgressMonitor monitor]
  (resources/listen-for-close (load-project-and-tools eclipse-project branch monitor)))

; ---------------------------------------------------------------------------
; Documentation
; ---------------------------------------------------------------------------
(doseq [[v doc]
       {*ns*
        "Functions for performing transactional changes to a project and inspecting its current state."

        #'open-project
        "Called from com.dynamo.cr.editor.Activator when opening a project. You should not call this function."

        #'register-filetype
        "TODO"

        #'register-loader
        "Associate a filetype (extension) with a loader function. The given loader will be
used any time a file with that type is opened."

        #'register-editor
        "TODO"

        #'load-resource
        "Load a resource, usually from file. This looks up a suitable loader based on the filename.
Loaders must be registered via register-loader before they can be used.

This will invoke the loader function with the filename and a reader to supply the file contents."

        #'node-by-filename
        "TODO"

        #'Project
        "TODO"
}]
  (alter-meta! v assoc :doc doc))
