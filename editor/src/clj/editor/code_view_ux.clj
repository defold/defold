(ns editor.code-view-ux
  (:require [dynamo.graph :as g]
            [editor.handler :as handler])
  (:import  [javafx.scene.input KeyEvent]))

(defprotocol TextContainer
  (text! [this s])
  (text [this]))

(defprotocol TextView
  (text-selection [this])
  (text-selection! [this offset length])
  (selection-offset [this])
  (selection-length [this])
  (caret [this])
  (caret! [this offset select?])
  (doc! [this s])
  (doc [s]))

(defprotocol TextStyles
  (styles [this]))

(def key-mappings
  {:Right :right
   :Left :left
   :Shift+Right :select-right})

(defn- info [e]
  {:key-code (.getCode ^KeyEvent e)
   :control? (.isControlDown ^KeyEvent e)
   :meta? (.isMetaDown ^KeyEvent e)
   :alt? (.isAltDown ^KeyEvent e)
   :shift? (.isShiftDown ^KeyEvent e)})

(defn- add-modifier [info modifier-key modifier-str code-str]
  (println "info" info)
  (println "modifier-key" modifier-key  "get " (get info modifier-key) )
  (if (get info modifier-key) (str modifier-str code-str) code-str))

(defn- key-fn [info code]
  (let [code (->> (.getName code)
                 (add-modifier info :meta? "Meta+")
                 (add-modifier info :alt? "Alt+")
                 (add-modifier info :control? "Control+")
                 (add-modifier info :shift? "Shift+")
                 (keyword))]
    (println "code" code)
    (get key-mappings code)))

(defn handle-key-pressed [e source-viewer]
  (let [k-info (info e)
        kf (key-fn k-info (.getCode ^KeyEvent e))]
    (println "Carin kf " kf)
    (when kf (handler/run
               kf
               [{:name :code-view :env {:selection source-viewer}}]
               k-info))))

(handler/defhandler :key-pressed :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (println "Carin selection " selection " user-data " user-data)))

(handler/defhandler :copy :code-view
  (enabled? [selection] selection)
  (run [selection clipboard]
    (text! clipboard (text-selection selection))))

(handler/defhandler :paste :code-view
  (enabled? [selection] selection)
  (run [selection clipboard]
    (when-let [clipboard-text (text clipboard)]
      (let [code (doc selection)
            caret (caret selection)
            new-code (str (.substring ^String code 0 caret)
                          clipboard-text
                          (.substring ^String code caret (count code)))
            new-caret (+ caret (count clipboard-text))]
        (doc! selection new-code)
        (caret! selection new-caret false)))))

(handler/defhandler :right :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)]
     (caret! selection (inc c) false))))

(handler/defhandler :left :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)]
     (caret! selection (dec c) false))))

(handler/defhandler :select-right :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)]
     (caret! selection (inc c) true))))

(handler/defhandler :select-left :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)]
     (caret! selection (dec c) true))))
