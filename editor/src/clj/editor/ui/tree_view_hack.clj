(ns editor.ui.tree-view-hack
  (:import
    (javafx.scene.control MultipleSelectionModel MultipleSelectionModelBase SelectionModel TreeView)))

(set! *warn-on-reflection* true)

;; HACK: Workaround for: https://bugs.openjdk.java.net/browse/JDK-8165222
;;
;; The BitSetReadOnlyUnbackedObservableList class keeps track of a
;; lastGetIndex and lastGetValue in order to be able to do some
;; performance optimizations. In some circumstances, these fields get
;; values that are inconsistent with the underlying BitSet for the
;; selected indices, causing the get method to incorrectly return -1,
;; which in turn causes exceptions.
;;
;; The subvert-broken-selection-model-optimization! function resets
;; the state of those fields to the initial state, thereby disabling
;; the broken performance optimizations.

;; The ReadOnlyUnbackedObservableList implementation used by the
;; TreeView selection model is an anonymous inner class in JDK
;; 1.8.0_101, and a named class in 1.8.0_111. If/when we upgrade we
;; need to change the class name.

(defmacro swallow [type & body]
  `(try ~@body (catch ~type ~'_)))

(def ^Class observable-list-class
  (or
    ;; JDK8u60-102
    (swallow ClassNotFoundException   
             (import 'javafx.scene.control.MultipleSelectionModelBase$4)
             (Class/forName "javafx.scene.control.MultipleSelectionModelBase$4"))
    ;; JDK8u111+
    (swallow ClassNotFoundException
             (import 'javafx.scene.control.MultipleSelectionModelBase$BitSetReadOnlyUnbackedObservableList)
             (Class/forName "javafx.scene.control.MultipleSelectionModelBase$BitSetReadOnlyUnbackedObservableList"))))

(def ^java.lang.reflect.Field selected-indices-seq-field
  (swallow Throwable
           (doto (.getDeclaredField MultipleSelectionModelBase "selectedIndicesSeq")
             (.setAccessible true))))

(def ^java.lang.reflect.Field last-get-index-field
  (swallow Throwable
           (doto (.getDeclaredField observable-list-class "lastGetIndex")
             (.setAccessible true))))

(def ^java.lang.reflect.Field last-get-value-field
  (swallow Throwable
           (doto (.getDeclaredField observable-list-class "lastGetValue")
             (.setAccessible true))))

(defn subvert-broken-selection-model-optimization!
  [^SelectionModel selection-model]
  (when (and (.isAssignableFrom MultipleSelectionModelBase (.getClass selection-model))
             selected-indices-seq-field
             last-get-index-field
             last-get-value-field)
    (let [indices-seq (.get selected-indices-seq-field selection-model)]
      (.set last-get-index-field indices-seq (int -1))
      (.set last-get-value-field indices-seq (int -1)))))
