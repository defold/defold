(ns internal.ui.editors.generic-editor
  (:import [org.eclipse.ui.part EditorPart])
  (:gen-class 
    :name internal.ui.editors.GenericEditor 
    :extends org.eclipse.ui.part.EditorPart
    :exposes-methods {init superInit}))

;; abstract methods declared by WorkbenchPart
(defn -createPartControl [this parent])
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
