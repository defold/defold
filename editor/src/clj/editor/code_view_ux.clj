(ns editor.code-view-ux
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
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
  (caret! [this offset select?]))

(defprotocol TextStyles
  (styles [this]))

(def key-mappings
  {:Right :right
   :Left :left
   :Shift+Right :select-right
   :Shift+Left :select-left
   :Up :up
   :Down :down})

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

(defn- in-bounds? [selection caret-pos]
  (and
   (not (neg? caret-pos))
   (>= (count (text selection)) caret-pos)))

(handler/defhandler :right :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          next-pos (inc c)]
      (when (in-bounds? selection next-pos)
         (caret! selection next-pos false)))))

(handler/defhandler :left :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          next-pos (dec c)]
      (when (in-bounds? selection next-pos)
         (caret! selection next-pos false)))))

(handler/defhandler :select-right :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          next-pos (inc c)]
      (when (in-bounds? selection next-pos)
         (caret! selection next-pos true)))))

(handler/defhandler :select-left :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          next-pos (dec c)]
      (when (in-bounds? selection next-pos)
         (caret! selection next-pos true)))))

(defn- adjust-bounds [s pos]
  (if (neg? pos) 0 (min (count s) pos)))

(defn up-line [s pos]
  (let [np (adjust-bounds s (inc pos))
        lines (string/split (.substring s 0 np) #"\n")
        len-before (-> lines
                       last
                       count)
        prev-line (-> lines reverse second)]
    (println :prev-line prev-line (if (pos? (count prev-line)) true false))
    (println :len-before len-before)
    (if (pos? (count prev-line))
      (adjust-bounds s (- pos (count prev-line) 1))
      (adjust-bounds s (- pos len-before 1)))))

(defn down-line [s pos]
  (let [np (adjust-bounds s pos)
        len-before (-> (string/split (.substring s 0 np) #"\n")
                       last
                       count)
        lines-after (string/split (.substring s np) #"\n")
        _ (println :lines-after lines-after)
        len-after (-> lines-after first count)
        _ (println :len-after len-after)
        _ (println :len-before len-before)
        next-line-len (-> lines-after second count)]
    (println :next-line-len next-line-len)
    (if (pos? next-line-len)
      (adjust-bounds s (+ pos len-after 1 len-before))
      (adjust-bounds s (+ pos len-after 1)))))

(handler/defhandler :up :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          doc (text selection)
          next-pos (up-line doc c)]
      (when (in-bounds? selection next-pos)
         (caret! selection next-pos false)))))

(handler/defhandler :down :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [c (caret selection)
          doc (text selection)
          next-pos (down-line doc c)]
      (when (in-bounds? selection next-pos)
         (caret! selection next-pos false)))))

(comment


  (def doc "line1\nline2\nline3")
  (count doc)
  (get doc 16)
  (get doc (up-line doc 4))
  (get doc (down-line doc 4))
  (second (reverse (string/split (.substring doc 0 (inc 10)) #"\n")

                   ))
  (count (last (string/split (.substring doc 0 (inc 10)) #"\n")))
  (count (first (string/split (.substring doc (inc 10)) #"\n")))
  )
