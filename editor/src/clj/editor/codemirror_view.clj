(ns editor.codemirror-view
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.handler :as handler]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
  (:import [javafx.scene Parent]
           [javafx.scene.input Clipboard ClipboardContent KeyEvent MouseEvent]
           [javafx.scene.image Image ImageView]
           [javafx.scene.web WebView WebEngine]
           [javafx.concurrent Worker Worker$State]
           [java.util.function Function]
           [javafx.scene.control ListView ListCell Tab Label]
           [netscape.javascript JSObject]))

(set! *warn-on-reflection* true)

(defprotocol CodeMirrorEditor
  (set-value! [this text]))

(definterface Bridge
  (onload [])
  (onchange []))

(defn setup-web-view!
  [^WebView web-view]
  (let [engine (.getEngine web-view)
        ^JSObject window (.executeScript engine "window")
        codemirror-instance (promise)]
    (.setMember window "java"
                (proxy [Bridge] []
                  (onload []
                    (println "onload"))
                  (onchange []
                    (println "change"))))
    
    (ui/observe (.. engine getLoadWorker stateProperty)
                (fn [prop old new]
                  (when (= new Worker$State/SUCCEEDED)
                    (println "LOADED YAY" (Thread/currentThread))
                    (try
                      (let [editor (.getMember window "editor")]
                        (println editor)
                        (deliver codemirror-instance editor))
                      (catch Throwable t
                        (prn t))))))
    
    ;; TODO: test if this works from jar
    (.load engine (.toExternalForm (clojure.java.io/resource "codemirror/codemirror.html")))
    #_(.load engine "http://localhost:12345/codemirror.html")

    (reify CodeMirrorEditor
      (set-value! [this text]
        (when-let [cm (deref codemirror-instance 200 nil)]
          (.call ^JSObject cm "setValue" (object-array [text])))))))


(g/defnk update-editor
  [codemirror-editor code]
  (println "update-editor")
  ;; hack
  (ui/run-later (set-value! codemirror-editor code)))

(g/defnode CodeView
  (property codemirror-editor g/Any)
  (input code-node g/Int)
  (input code g/Str)
  (input caret-position g/Int)
  (input prefer-offset g/Int)
  (input tab-triggers g/Any)
  (input selection-offset g/Int)
  (input selection-length g/Int)
  (output new-content g/Any :cached update-editor))

(defn setup-code-view [app-view-id view-id code-node initial-caret-position]
  (g/transact
   (concat
    (g/connect code-node :_node-id view-id :code-node)
    (g/connect code-node :code view-id :code)
    ;; (g/connect code-node :caret-position view-id :caret-position)
    ;; (g/connect code-node :prefer-offset view-id :prefer-offset)
    ;; (g/connect code-node :tab-triggers view-id :tab-triggers)
    ;; (g/connect code-node :selection-offset view-id :selection-offset)
    ;; (g/connect code-node :selection-length view-id :selection-length)
    (g/set-property code-node :caret-position initial-caret-position)
    (g/set-property code-node :prefer-offset 0)
    (g/set-property code-node :tab-triggers nil)
    (g/set-property code-node :selection-offset 0)
    (g/set-property code-node :selection-length 0)))
  view-id)

(defn make-view [graph ^Parent parent code-node opts]
  (let [^Parent editor (ui/load-fxml "codemirror.fxml")
        controls (ui/collect-controls editor ["webview"])
        codemirror-editor (setup-web-view! (:webview controls))
        view-id (setup-code-view (:app-view opts) (g/make-node! graph CodeView :codemirror-editor codemirror-editor) code-node (get opts :caret-position 0))]
    (ui/children! parent [editor])
    (ui/fill-control editor)
    
    #_(ui/context! source-viewer :code-view {:code-node code-node :view-node view-id :clipboard (Clipboard/getSystemClipboard)} source-viewer)
    #_(ui/observe (.selectedProperty ^Tab (:tab opts)) (fn [this old new]
                                                         (when (= true new)
                                                           (ui/run-later (cvx/refresh! source-viewer)))))
    (g/node-value view-id :new-content)
    view-id))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :code
                                :label "Code"
                                :make-view-fn (fn [graph ^Parent parent code-node opts]
                                                (make-view graph parent code-node opts))))
