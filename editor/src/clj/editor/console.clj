(ns editor.console
  (:require [editor.ui :as ui]
            [clojure.string :as str])
  (:import [javafx.scene.control Button TextArea TextField]))

(set! *warn-on-reflection* true)

(defonce ^:private node (atom nil))
(defonce ^:private term (atom ""))
(defonce ^:private positions (atom '()))

(defn- update-highlight [idx]
  (when-let [^TextArea node @node]
    (ui/run-later
     (if (and idx (not= -1 idx))
       (.selectRange node idx (+ idx (count @term)))
       (.selectRange node 0 0)))))

(defn- next-match []
  (when-let [^TextArea node @node]
    (.indexOf (str/lower-case (.getText node))
              (str/lower-case @term)
              ^Long (inc (or (first @positions) -1)))))

(defn- clear-console! [_]
  (when-let [^TextArea node @node]
    (reset! term "")
    (reset! positions '())
    (ui/run-later (.clear node))))

(defn- search-console [_ _ ^String new]
  (reset! term new)
  (reset! positions '())
  (let [idx (next-match)]
    (update-highlight idx)
    (when (not= -1 idx)
      (swap! positions conj idx))))

(defn- next-console! [_]
  (let [idx (next-match)]
    (when (not= -1 idx)
      (swap! positions conj idx)
      (update-highlight idx))))

(defn- prev-console! [_]
  (when (> (count @positions) 1)
    (swap! positions rest))
  (update-highlight (first @positions)))

(defn append-console-message! [message]
  (when-let [^TextArea node @node]
    (ui/run-later
     (.appendText node message))))

(defn setup-console! [{:keys [^TextArea text ^TextField search ^Button prev ^Button next ^Button clear]}]
  (ui/on-action! clear clear-console!)
  (ui/on-action! next next-console!)
  (ui/on-action! prev prev-console!)
  (ui/observe (.textProperty search) search-console)
  (reset! node text)
  (append-console-message! "Welcome to Defold!\n"))
