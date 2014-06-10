(ns internal.ui.editors.atlas-editor
  (:require [internal.ui.editors :refer [swt-safe]])
  (:import [org.eclipse.ui.part EditorPart])
  (:gen-class 
    :name internal.ui.editors.AtlasEditor 
    :extends org.eclipse.ui.part.EditorPart))

(def text (atom "Hello, world!"))

;; abstract methods declared by WorkbenchPart
(defn -createPartControl [this parent]
  (let [l (doto (org.eclipse.swt.widgets.Label. parent org.eclipse.swt.SWT/WRAP)
            (.setText @text))]
    (add-watch text :text (fn [_ _ _ new-text] (swt-safe (.setText l new-text))))
    ))

(defn -setFocus [this])

;; abstract methods declared by EditorPart
(defn -doSave [this progress-monitor])
(defn -doSaveAs [this])

(defn -init [this site input]
  (println site)
  (.setSite this site)
  (println input)
  (.setInput this input))

(defn -isDirty [this]
  false)

(defn -isSaveAsAllowed [this]
  false)
