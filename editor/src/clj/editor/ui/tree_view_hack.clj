(ns editor.ui.tree-view-hack
  ;; NOTE: The ReadOnlyUnbackedObservableList implementation used by
  ;; the TreeView selection model is an anonymous inner class in JDK
  ;; 1.8.0_101, and a named class in 1.8.0_112. If/when we upgrade we
  ;; need to change the class name.
  (:import (javafx.scene.control MultipleSelectionModelBase
                                 MultipleSelectionModelBase$4
                                 ;; 1.8.0_111/112: MultipleSelectionModelBase$BitSetReadOnlyUnbackedObservableList
                                 )))

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

(def ^java.lang.reflect.Field selected-indices-seq-field
  (doto (.getDeclaredField MultipleSelectionModelBase "selectedIndicesSeq")
    (.setAccessible true)))

(def ^java.lang.reflect.Field last-get-index-field
   (doto (.getDeclaredField MultipleSelectionModelBase$4 "lastGetIndex")
    (.setAccessible true)))

(def ^java.lang.reflect.Field last-get-value-field
  (doto (.getDeclaredField MultipleSelectionModelBase$4 "lastGetValue")
    (.setAccessible true)))

(defn subvert-broken-selection-model-optimization!
  [selection-model]
  (when (.isAssignableFrom MultipleSelectionModelBase (.getClass selection-model))
    (let [indices-seq (.get selected-indices-seq-field selection-model)]
      (.set last-get-index-field indices-seq (int -1))
      (.set last-get-value-field indices-seq (int -1)))))
