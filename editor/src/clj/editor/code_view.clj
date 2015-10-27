(ns editor.code-view
  (:require [dynamo.graph :as g]
            [internal.system :as is]
            [editor.core :as core]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
  (:import [javafx.scene Parent]
           [javafx.scene.control TextArea]
           [javafx.animation AnimationTimer]
           [com.defold.control UndolessTextArea]))

(defn- opseqs [text-area]
  (ui/user-data text-area ::opseqs))

(defn- new-opseq [text-area]
  (swap! (opseqs text-area) rest))

(defn- opseq [text-area]
  (first @(opseqs text-area)))

(defn- programmatic-change [text-area]
  (ui/user-data text-area ::programmatic-change))

(defn- code-node [text-area]
  (ui/user-data text-area ::code-node))

(defn- restart-opseq-timer [text-area]
  (if-let [timer (ui/user-data text-area ::opseq-timer)]
    (.playFromStart timer)
    (ui/user-data! text-area ::opseq-timer (ui/->future 0.5 #(new-opseq text-area)))))

(defmacro binding-atom [a val & body]
  `(let [old-val# (deref ~a)]
     (try
       (reset! ~a ~val)
       ~@body
       (finally
         (reset! ~a old-val#)))))

(defn- setup-text-area [text-area]
  (ui/user-data! text-area ::opseqs (atom (repeatedly gensym)))
  (ui/user-data! text-area ::programmatic-change (atom nil))
  (ui/observe (.textProperty text-area)
    (fn [_ _ new-code]
      (restart-opseq-timer text-area)
      (when-not @(programmatic-change text-area)
        (g/transact
          (concat
            (g/operation-sequence (opseq text-area))
            (g/operation-label (str "Text Change: " new-code))
            (g/set-property (code-node text-area) :code new-code))))))
  (ui/observe (.caretPositionProperty text-area)
    (fn [_ _ new-caret-position]
      (when-not @(programmatic-change text-area)
        (g/transact
          (concat
            (g/operation-sequence (opseq text-area))
            ;; we dont supply an operation label here, because the caret position events comes just after the text change, and it seems
            ;; when merging transactions the latest label wins
            (g/set-property (code-node text-area) :caret-position new-caret-position))))))
  text-area)

(g/defnk update-text-area [^TextArea text-area code-node code caret-position]
  (ui/user-data! text-area ::code-node code-node)
  (when (not= code (.getText text-area))
    (new-opseq text-area)
    (binding-atom (programmatic-change text-area) true
      (.setText text-area code)))
  (when (not= caret-position (.getCaretPosition text-area))
    (new-opseq text-area)
    (binding-atom (programmatic-change text-area) true
      (.positionCaret text-area caret-position)))
  [code-node code caret-position])

(g/defnode CodeView
  (property text-area TextArea)
  (input code-node g/Int)
  (input code g/Str)
  (input caret-position g/Int)
  (output new-content g/Any :cached update-text-area))

(defn- setup-code-view [view-id code-node]
  (g/transact
    (concat
      (g/connect code-node :_node-id view-id :code-node)
      (g/connect code-node :code view-id :code)
      (g/connect code-node :caret-position view-id :caret-position)))
  view-id)

(defn make-view [graph ^Parent parent code-node opts]
  (let [text-area (setup-text-area (UndolessTextArea.))
        view-id (setup-code-view (g/make-node! graph CodeView :text-area text-area) code-node)]
    (ui/children! parent [text-area])
    (ui/fill-control text-area)
    (let [refresh-timer (ui/->timer 10 (fn [_] (g/node-value view-id :new-content)))
          stage (.. parent getScene getWindow)]
      (ui/timer-stop-on-close! stage refresh-timer)
      (ui/timer-start! refresh-timer)
      view-id)))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :code
                                :make-view-fn make-view))
