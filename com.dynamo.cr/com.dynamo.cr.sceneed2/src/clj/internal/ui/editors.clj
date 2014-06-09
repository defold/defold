(ns internal.ui.editors
  (:require [service.log :as log]
            [service.registry :refer [registered]])
  (:import [org.eclipse.ui PlatformUI]
           [org.eclipse.ui.internal.registry FileEditorMapping]
           [org.eclipse.swt.widgets Display])
  (:use [clojure.pprint]))

(defn swt-thread-safe*
  [f]
  (.asyncExec (or (Display/getCurrent) (Display/getDefault)) f))

(defmacro swt-safe
  [& body]
  `(swt-thread-safe* (fn [] (do ~@body))))

(defrecord EditorAssociation [id filetypes fn-var args])

(defn create-editor-association
  [filetypes fn-var args]
  (EditorAssociation. "com.dynamo.cr.sceneed2.sceneEditor" filetypes fn-var args))

(defn- remove-editor-from-eclipse
  [^EditorAssociation editor]
  
  )

(defn- editor-registry
  []
  (.. PlatformUI getWorkbench getEditorRegistry))

(defn- editor-mappings
  []
  (seq (.getFileEditorMappings (editor-registry))))

(defn- update-editor-mappings!
  [mappings]
  (swt-safe
    (.setFileEditorMappings (editor-registry) (into-array FileEditorMapping mappings))))

(defn file-editor-mapping
  [file-ext editor]
  (let [descriptor (.findEditor (editor-registry) (:id editor))]
    (doto (FileEditorMapping. file-ext)
      (.setDefaultEditor descriptor))))

(defn- add-editor-to-eclipse
  [editor]
  (let [new-mappings (reduce (fn [mappings file-ext] (conj mappings (file-editor-mapping file-ext editor))) 
                               (editor-mappings) 
                               (:filetypes editor))]
    (update-editor-mappings! new-mappings)))

(def make-editor
  (registered
    (fn [filetypes fn-var & args]
      (add-editor-to-eclipse (create-editor-association filetypes fn-var args)))
    (fn [filetypes fn-var & args]
      [filetypes (str fn-var)])
    (fn [^EditorAssociation entry]
      [(:filetypes entry) (str (:fn-var entry))])
    (fn [entry]
      (remove-editor-from-eclipse entry))))