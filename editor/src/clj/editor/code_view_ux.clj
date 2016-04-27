(ns editor.code-view-ux
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.handler :as handler])
  (:import  [javafx.scene.input KeyEvent]))

(defprotocol TextContainer
  (text! [this s])
  (text [this]))

(defprotocol TextScroller
  (preferred-offset [this])
  (preferred-offset! [this offset]))

(defprotocol TextView
  (text-selection [this])
  (text-selection! [this offset length])
  (selection-offset [this])
  (selection-length [this])
  (caret [this])
  (caret! [this offset select?]))

(defprotocol TextStyles
  (styles [this]))

(def key-mappings
  {:Right :right
   :Left :left
   :Shift+Right :select-right
   :Shift+Left :select-left
   :Up :up
   :Down :down
   })

(defn- info [e]
  {:key-code (.getCode ^KeyEvent e)
   :control? (.isControlDown ^KeyEvent e)
   :meta? (.isMetaDown ^KeyEvent e)
   :alt? (.isAltDown ^KeyEvent e)
   :shift? (.isShiftDown ^KeyEvent e)})

(defn- add-modifier [info modifier-key modifier-str code-str]
  (if (get info modifier-key) (str modifier-str code-str) code-str))

;; potentially slow with each new keyword that is created
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
    (when kf (handler/run
               kf
               [{:name :code-view :env {:selection source-viewer}}]
               k-info))))

(handler/defhandler :copy :code-view
  (enabled? [selection] selection)
  (run [selection clipboard]
    (text! clipboard (text-selection selection))))

(handler/defhandler :paste :code-view
  (enabled? [selection] selection)
  (run [selection clipboard]
    (when-let [clipboard-text (text clipboard)]
      (let [code (text selection)
            caret (caret selection)
            new-code (str (.substring ^String code 0 caret)
                          clipboard-text
                          (.substring ^String code caret (count code)))
            new-caret (+ caret (count clipboard-text))]
        (text! selection new-code)
        (caret! selection new-caret false)))))

(defn- adjust-bounds [s pos]
  (if (neg? pos) 0 (min (count s) pos)))

(handler/defhandler :right :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          doc (text selection)
          next-pos (adjust-bounds doc (inc c))]
      (caret! selection next-pos false))))

(handler/defhandler :left :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          doc (text selection)
          next-pos (adjust-bounds doc (dec c))]
      (caret! selection next-pos false))))

(handler/defhandler :select-right :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          doc (text selection)
          next-pos (adjust-bounds doc (inc c))]
      (caret! selection next-pos true))))

(handler/defhandler :select-left :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          doc (text selection)
          next-pos (adjust-bounds doc (dec c))]
      (caret! selection next-pos true))))

(defn lines-before [s pos]
  (let [np (adjust-bounds s pos)]
    ;; an extra char is put in to pick up mult newlines
    (string/split (str (.substring s 0 np) "x") #"\n")))

(defn lines-after [s pos]
  (let [np (adjust-bounds s pos)]
    (string/split (.substring s np) #"\n")))

(defn up-line [s pos preferred-offset]
  (let [lines-before (lines-before s pos)
        len-before (-> lines-before last count dec)
        prev-line-len (-> lines-before reverse second count)
        preferred-next-pos (+ (- pos len-before 1 prev-line-len)
                              (min preferred-offset prev-line-len))]
    (adjust-bounds s preferred-next-pos)))

(defn down-line [s pos preferred-offset]
  (let [lines-after (lines-after s pos)
        len-after (-> lines-after first count)
        next-line-len (-> lines-after second count)
        preferred-next-pos (+ pos len-after 1 (min preferred-offset next-line-len))]
      (adjust-bounds s preferred-next-pos)))

(handler/defhandler :up :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          doc (text selection)
          preferred-offset (preferred-offset selection)
          next-pos (up-line doc c preferred-offset)]
      (caret! selection next-pos false))))

(handler/defhandler :down :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          doc (text selection)
          preferred-offset (preferred-offset selection)
          next-pos (down-line doc c preferred-offset)]
      (caret! selection next-pos false))))
