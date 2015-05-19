(ns editor.diff-view
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [editor.ui :as ui]
            [service.log :as log])
  (:import [javafx.scene Parent Scene]
           [javafx.stage Stage]
           [javafx.scene.paint Paint]
           [javafx.scene.layout Pane]
           [javafx.scene.shape Line Rectangle]
           [javafx.scene.text Font Text]
           [javafx.fxml FXMLLoader]
           [org.eclipse.jgit.diff Edit Edit$Type HistogramDiff RawTextComparator RawText]))

(defn- diff-strings [^String str-left ^String str-right]
  (.diff (HistogramDiff.) RawTextComparator/DEFAULT (RawText. (.getBytes str-left)) (RawText. (.getBytes str-right))))

(def edit-type->keyword {Edit$Type/REPLACE :replace Edit$Type/DELETE :delete Edit$Type/INSERT :insert Edit$Type/EMPTY :empty})

(defn- edit->map [^Edit edit]
  {:left {:begin (.getBeginA edit) :end (.getEndA edit)}
   :right {:begin (.getBeginB edit) :end (.getEndB edit)}
   :type (get edit-type->keyword (.getType edit))})

; TODO: \r and windows?
(defn- split-lines [str]
  (vec (str/split str #"\n")))

(defn- cut-edit [edit cutter]
  [(-> edit
     (assoc-in [:left :end]  (get-in cutter [:left :begin]))
     (assoc-in [:right :end] (get-in cutter [:right :begin])))
   cutter
   (-> edit
     (assoc-in [:left :begin] (get-in cutter [:left :end]))
     (assoc-in [:right :begin] (get-in cutter [:right :end])))])

(defn- redundant? [{left :left right :right type :type}]
  (and (= type :nop)
       (= (:begin left) (:end left))
       (= (:begin right) (:end right))))

(defn- insert-nop-edits [nop-edit edits]
  (let [[nop-edit' edits'] (->> edits (reduce (fn [[nop-edit' edits'] edit]
                                                (let [[a b c] (cut-edit nop-edit' edit)]
                                                  [c (conj edits' a b)]))
                                              [nop-edit []]))]
    (conj edits' nop-edit')))

(defn diff [^String str-left ^String str-right]
  (let [edits (map edit->map (diff-strings str-left str-right))
        lines-left (split-lines str-left)
        lines-right (split-lines str-right)
        nop-edit {:left {:begin 0 :end (count lines-left)}
                  :right {:begin 0 :end (count lines-right)}
                  :type :nop}
        edits' (insert-nop-edits nop-edit edits)]
    (remove redundant? edits')))

(defn- make-text [font lines {begin :begin end :end}]
  (let [s (str/join "\n" (subvec lines begin end))
        t (Text. s)]
    (.setFont t font)
    (if (= begin end) (Rectangle. 100000 0) t)))

(defn- make-texts [^Pane box lines edits selector]
  (let [font (Font. "Courier New" 13)
        texts (mapv (fn [e] (make-text font lines (selector e))) edits)
        heights (reductions (fn [sum t] (+ sum (.getHeight (.getBoundsInLocal t)))) 0 texts)]
    (doseq [[t y] (map vector texts heights)]
      (.setY t (+ (- (.getMinY (.getBoundsInLocal t))) y))
      (.add (.getChildren box) t))
    texts))

(defn- make-boxes [^Pane box texts edits selector]
  (doseq [[text edit] (map vector texts edits)]
    (when-not (= (:type edit) :nop)
      (let [b (.getBoundsInLocal text)
            r (Rectangle. (.getMinX b) (.getMinY b) (.getWidth b) (.getHeight b))]
        (.setFill r (Paint/valueOf "#cccccc55"))
        (.setStroke r (Paint/valueOf "#000000"))
        (.bind (.widthProperty r) (.widthProperty box))
        (.add (.getChildren box) r)))))

(defn- make-line [^Text r1 ^Text r2 left right]
  (let [b1 (.getBoundsInLocal r1)
        b2 (.getBoundsInLocal r2)]
    (Line. left (+ (.getMinY b1) (* 0.5 (.getHeight b1))) right (+ (.getMinY b2) (* 0.5 (.getHeight b2))))))

(defn- make-lines [^Pane box texts-left texts-right edits]
  (doseq [[t-left t-right edit] (map vector texts-left texts-right edits)]
    (when-not (= (:type edit) :nop)
      (let [line (make-line t-left t-right 1 40)]
        (.add (.getChildren box) line)))))

(defn- make-diff-viewer [str-left str-right]
  (let [root ^Parent (FXMLLoader/load (io/resource "diff.fxml"))
        stage (Stage.)
        scene (Scene. root)
        lines-left (split-lines str-left)
        lines-right (split-lines str-right)
        edits (diff str-left str-right)]
    (.setScene stage scene)
    (.show stage)

    (let [^Pane left (.lookup root "#left")
          ^Pane right (.lookup root "#right")
          ^Pane markers (.lookup root "#markers")
          texts-left (make-texts left lines-left edits :left)
          texts-right (make-texts right lines-right edits :right)]
      (make-boxes left texts-left edits :left)
      (make-boxes right texts-right edits :right)
      (make-lines markers texts-left texts-right edits))))

; TODO: Remove soon
#_(ui/run-later (make-diff-viewer "1\n2\n3\n4\n5\n6\n7\n8....\nAPA1\n...\n...\n...\n...\n...\n...\n...\n...\n...\n...\n"
                              "XXX\n2\n3\n4\n6\n7\n8\nAPA1\n...\n...\n...\n...\n...\n...\n...\n...\n...\n...\nNEW STUFF\n"))
#_(ui/run-later (make-diff-viewer (slurp "a.txt") (slurp "b.txt")))
