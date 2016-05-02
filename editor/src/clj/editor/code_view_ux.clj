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
   :Control+Right :next-word
   :Alt+Right :next-word
   :Control+Left :prev-word
   :Alt+Left :prev-word
   :Meta+Left :line-begin
   :Control+A :line-begin
   :Meta+Right :line-end
   :Control+E :line-end
   :Meta+Up :file-begin
   :Meta+Down :file-end
   })

(def tab-size 4)

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
            new-code (str (subs code 0 caret)
                          clipboard-text
                          (subs code caret (count code)))
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

(defn tab-count [s]
  (let [tab-count (count (filter #(= \tab %) s))]
    tab-count))

(defn lines-before [s pos]
  (let [np (adjust-bounds s pos)]
    ;; an extra char is put in to pick up mult newlines
    (string/split (str (subs s 0 np) "x") #"\n")))

(defn lines-after [s pos]
  (let [np (adjust-bounds s pos)]
    (string/split (subs s np) #"\n")))

(defn up-line [s pos preferred-offset]
  (let [lines-before (lines-before s pos)
        len-before (-> lines-before last count dec)
        prev-line-tabs (-> lines-before drop-last last tab-count)
        prev-line-len (-> lines-before drop-last last count)
        preferred-next-pos (min preferred-offset (+ prev-line-len (* prev-line-tabs (dec tab-size))))
        actual-next-pos (- preferred-next-pos (* prev-line-tabs (dec tab-size)))
        adj-next-pos (+ (- pos len-before 1 prev-line-len)
                        (if (neg? actual-next-pos) 0 actual-next-pos))]
      (adjust-bounds s adj-next-pos)))

(defn down-line [s pos preferred-offset]
  (let [lines-after (lines-after s pos)
        len-after (-> lines-after first count)
        next-line-tabs (-> lines-after second tab-count)
        next-line-len (-> lines-after second count)
        preferred-next-pos (min preferred-offset (+ next-line-len (* next-line-tabs (dec tab-size))))
        actual-next-pos (- preferred-next-pos (* next-line-tabs (dec tab-size)))
        adj-next-pos (+ pos len-after 1 (if (neg? actual-next-pos) 0 actual-next-pos))]
      (adjust-bounds s adj-next-pos)))

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

(def word-regex  #"\n|\w+|\s\w+|.")

(handler/defhandler :next-word :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          doc (text selection)
          np (adjust-bounds doc c)
          next-word-move (re-find word-regex (subs doc np))
          next-pos (+ np (count next-word-move))]
      (caret! selection next-pos false))))

(handler/defhandler :prev-word :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          doc (text selection)
          np (adjust-bounds doc c)
          next-word-move (re-find word-regex (->> (subs doc 0 np) (reverse) (apply str)))
          next-pos (- np (count next-word-move))]
      (caret! selection next-pos false))))

(handler/defhandler :line-begin :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          doc (text selection)
          np (adjust-bounds doc c)
          lines-before (lines-before doc np)
          len-before (-> lines-before last count dec)
          next-pos (- np len-before)]
      (caret! selection next-pos false))))

(handler/defhandler :line-end :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          doc (text selection)
          np (adjust-bounds doc c)
          lines-after (lines-after doc np)
          len-after (-> lines-after first count)
          next-pos (+ np len-after)]
      (caret! selection next-pos false))))

(handler/defhandler :file-begin :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (caret! selection 0 false)))

(handler/defhandler :file-end :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [doc (text selection)]
      (caret! selection (count doc) false))))
