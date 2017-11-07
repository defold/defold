(ns editor.code-view-ux
  (:require [clojure.string :as string]
            [editor.code :as code]
            [editor.dialogs :as dialogs]
            [editor.handler :as handler]
            [editor.ui :as ui]
            [editor.util :as util]
            [util.text-util :as text-util])
  (:import  [com.sun.javafx.tk Toolkit]
            [java.util.regex Pattern Matcher]
            [javafx.stage Stage]
            [javafx.beans.property SimpleStringProperty SimpleBooleanProperty]
            [javafx.scene.input Clipboard KeyEvent KeyCode MouseEvent]))

(set! *warn-on-reflection* true)

(defprotocol TextContainer
  (text! [this s])
  (text [this])
  (replace! [this offset length s]))

(defprotocol TextCaret
  (caret [this])
  (caret! [this offset select?]))

(defprotocol TextLine
  (prev-line [this])
  (prev-line-num [this])
  (next-line [this])
  (next-line-num [this])
  (line [this])
  (line-offset [this])
  (line-num-at-offset [this offset])
  (line-at-num  [this line-num])
  (line-offset-at-num [this line-num])
  (line-count [this]))

(defprotocol TextOffset
  (preferred-offset [this])
  (preferred-offset! [this val]))

(defprotocol TextSnippet
  (snippet-tab-triggers [this])
  (snippet-tab-triggers! [this val])
  (has-snippet-tab-trigger? [this])
  (has-prev-snippet-tab-trigger? [this])
  (next-snippet-tab-trigger! [this])
  (prev-snippet-tab-trigger! [this])
  (clear-snippet-tab-triggers! [this]))

(defprotocol TextView
  (text-selection [this])
  (text-selection! [this offset length])
  (selection-offset [this])
  (selection-length [this])
  (editable? [this])
  (editable! [this val])
  (screen-position [this])
  (text-area [this])
  (refresh! [this])
  (page-up [this])
  (page-down [this])
  (last-visible-row-number [this])
  (first-visible-row-number [this])
  (show-line [this]))

(defprotocol TextStyles
  (styles [this]))

(defprotocol TextUndo
  (typing-changes! [this] [this merge-changes])
  (state-changes! [this]))

(defprotocol CommandHistory
  (last-command [this])
  (last-command! [this cmd]))

(defprotocol TextProposals
  (propose [this]))

;; NOTE: These are effectively unused, and only here for historical reasons.
(def mappings
  ;; Shortcut is Ctrl on Windows, so mapping Shortcut+X will shadow Ctrl+X.
  {
  ;;;movement
  :Up                    {:command :up}
  :Down                  {:command :down}
  :Left                  {:command :left}
  :Right                 {:command :right}
  :Page-Up               {:command :page-up}
  :Page-Down             {:command :page-down}

  :Ctrl+Right            {:command :next-word}
  :Alt+Right             {:command :next-word}
  :Ctrl+Left             {:command :prev-word}
  :Alt+Left              {:command :prev-word}
  :Shortcut+Left         {:command :beginning-of-line-text}
  :Ctrl+A                {:command :beginning-of-line}
  :Home                  {:command :beginning-of-line-text}
  :Shortcut+Right        {:command :end-of-line}
  :Ctrl+E                {:command :end-of-line}
  :End                   {:command :end-of-line}
  :Shortcut+Up           {:command :beginning-of-file}
  :Shortcut+Down         {:command :end-of-file}
  :Shortcut+L            {:command :goto-line}

  ;; select
  :Double-Click          {:command :select-word}
  :Triple-Click          {:command :select-line}
  :Shortcut+A            {:command :select-all}

  ;;movement with select
  :Shift+Right           {:command :select-right}
  :Shift+Left            {:command :select-left}
  :Shift+Up              {:command :select-up}
  :Shift+Down            {:command :select-down}
  :Shift+Alt+Right       {:command :select-next-word}
  :Shift+Ctrl+Right      {:command :select-next-word}
  :Shift+Alt+Left        {:command :select-prev-word}
  :Shift+Ctrl+Left       {:command :select-prev-word}
  :Shift+Shortcut+Left   {:command :select-beginning-of-line-text}
  :Shift+Ctrl+A          {:command :select-beginning-of-line}
  :Shift+Home            {:command :select-beginning-of-line-text}
  :Shift+Shortcut+Right  {:command :select-end-of-line}
  :Shift+Ctrl+E          {:command :select-end-of-line}
  :Shift+End             {:command :select-end-of-line}
  :Shift+Shortcut+Up     {:command :select-beginning-of-file}
  :Shift+Shortcut+Down   {:command :select-end-of-file}
  :Shift+Page-Up         {:command :select-page-up}
  :Shift+Page-Down       {:command :select-page-down}


  ;; find
  :Shortcut+F            {:command :find-text}
  :Shortcut+G            {:command :find-next}
  :Shift+Shortcut+G      {:command :find-prev}

  ;; Replace
  :Shortcut+E            {:command :replace-text}
  :Alt+Shortcut+E        {:command :replace-next}

  ;; Delete
  :Backspace             {:command :delete-backward}
  :Delete                {:command :delete}
  :Ctrl+D                {:command :delete-line}
  :Alt+Delete            {:command :delete-next-word}           ;; these two do not work when they are included in the menu
  :Alt+Backspace         {:command :delete-prev-word}           ;; the menu event does not get propagated back like the rest
  :Shortcut+Delete       {:command :delete-to-beginning-of-line}
  :Shift+Shortcut+Delete {:command :delete-to-end-of-line}
  :Ctrl+K                {:command :cut-to-end-of-line}

  ;; Comment
  :Shortcut+Slash        {:command :toggle-comment}

  ;; Editing
  :Tab                   {:command :tab}
  :Shift+Tab             {:command :backwards-tab-trigger}
  :Enter                 {:command :enter}

  ;; Paste
  :Ctrl+Y                {:command :paste}

  ;;Completions
  :Ctrl+Space            {:command :proposals}

  ;;Indentation
  :Shortcut+I            {:command :indent}
})


;; Finding overlaps...
;; 
;;  (def overlapping-mappings
;;    (let [combos (map (comp #(string/split % #"[+]") name) (keys mappings))
;;          ctrl-common (map set (map (partial replace {"Ctrl" "Ctrl/Shortcut"}) combos))
;;          shortcut-common (map set (map (partial replace {"Shortcut" "Ctrl/Shortcut"}) combos))]
;;      (filter #(contains? % "Ctrl/Shortcut") (clojure.set/intersection (set ctrl-common) (set shortcut-common)))))

(def proposal-key-triggers #{"."})

;; keep regex's in split-indentation etc in sync with these
(def tab-size 4)
(def default-indentation "\t")

;; these are global for now, reasonable to find/replace same thing in several files
(defonce ^SimpleStringProperty find-term (doto (SimpleStringProperty.) (.setValue "")))
(defonce ^SimpleStringProperty find-replacement (doto (SimpleStringProperty.) (.setValue "")))
(defonce ^SimpleBooleanProperty find-whole-word (doto (SimpleBooleanProperty.) (.setValue false)))
(defonce ^SimpleBooleanProperty find-case-sensitive (doto (SimpleBooleanProperty.) (.setValue false)))
(defonce ^SimpleBooleanProperty find-wrap (doto (SimpleBooleanProperty.) (.setValue false)))

(defn- get-find-params []
  {:term (.getValue find-term)
   :replacement (.getValue find-replacement)
   :whole-word (.getValue find-whole-word)
   :case-sensitive (.getValue find-case-sensitive)
   :wrap (.getValue find-wrap)})

(defn- create-find-expr [{:keys [term whole-word case-sensitive]}]
  (when (seq term)
    (cond-> (Pattern/quote term)
      whole-word (#(str "\\b" % "\\b"))
      (not case-sensitive) (#(str "(?i)" %)))))

(defn set-find-term [text]
  (.setValue find-term (or text "")))

(def auto-matches {"\"" "\""
                   "[" "]"
                   "{" "}"
                   "(" ")"})
(def auto-match-open-set (into #{} (keys auto-matches)))
(def auto-match-close-set (into #{} (vals auto-matches)))

(def auto-delete-set (into #{} (map (fn [[k v]] (str k v)) auto-matches)))

(defn- info [e]
  {:event e
   :key-code (.getCode ^KeyEvent e)
   :control? (.isControlDown ^KeyEvent e)
   :shortcut? (.isShortcutDown ^KeyEvent e)
   :alt? (.isAltDown ^KeyEvent e)
   :shift? (.isShiftDown ^KeyEvent e)})

(defn- add-modifier [code-str info modifier-key modifier-str]
  (if (get info modifier-key) (str modifier-str code-str) code-str))

(def shortcut-is-ctrl? (= (.getPlatformShortcutKey (Toolkit/getToolkit)) KeyCode/CONTROL))

;; potentially slow with each new keyword that is created
(defn- key-fn [info]
  (let [key-code-name (-> ^KeyCode (:key-code info)
                          (.getName)
                          (string/replace #" " "-"))]
    (if shortcut-is-ctrl?
      (let [shortcut-chord (-> key-code-name
                               (add-modifier info :shortcut? "Shortcut+")
                               (add-modifier info :alt? "Alt+")
                               (add-modifier info :shift? "Shift+")
                               (keyword))
            ctrl-chord (-> key-code-name
                           (add-modifier info :alt? "Alt+")
                           (add-modifier info :control? "Ctrl+")
                           (add-modifier info :shift? "Shift+")
                           (keyword))]
        (or (get mappings shortcut-chord)
            (get mappings ctrl-chord)))
      (let [key-chord (-> key-code-name
                          (add-modifier info :shortcut? "Shortcut+")
                          (add-modifier info :alt? "Alt+")
                          (add-modifier info :control? "Ctrl+")
                          (add-modifier info :shift? "Shift+")
                          (keyword))]
        (get mappings key-chord)))))

(defn- click-fn [click-count]
  (let [code (case (int click-count)
               3 :Triple-Click
               2 :Double-Click
               :Single-Click)]
    (get mappings code)))

(defn- handler-context [source-viewer]
  (ui/context source-viewer))

(defn handler-run [command command-contexts user-data]
  (let [command-contexts (handler/eval-contexts command-contexts true)
        handler-context (handler/active command command-contexts user-data)]
    (when (handler/enabled? handler-context)
      (handler/run handler-context)
      :handled)))

(defn handle-key-pressed [^KeyEvent e source-viewer]
  (let [k-info (info e)
        kf (key-fn k-info)]
    (when (and (not (.isConsumed e)) (:command kf) (not (:label kf)))
      (handler-run
       (:command kf)
       [(handler-context source-viewer)]
       k-info)
      (.consume e)
      (last-command! source-viewer (:command kf)))))

(defn handle-key-typed [^KeyEvent e source-viewer]
  (let [key-typed (.getCharacter e)]
    (when (handler-run
            :key-typed
            [(handler-context source-viewer)]
            {:key-typed key-typed :key-event e})
      (.consume e)
      (last-command! source-viewer :key-typed))))

(defn adjust-bounds [s pos]
  (if (neg? pos) 0 (min (count s) pos)))

(defn tab-count [s]
  (let [tab-count (count (filter #(= \tab %) s))]
    tab-count))

(defn- tab-expanded-count [s]
  (let [tc (tab-count s)]
    (+ (* tc tab-size)
       (- (count s) tc))))

(defn remember-caret-col [source-viewer np]
  (let [line-offset (line-offset source-viewer)
        line-text (line source-viewer)
        text-before (subs line-text 0 (min (count line-text) (- np line-offset)))]
    (preferred-offset! source-viewer (tab-expanded-count text-before))))

(defn handle-mouse-clicked [^MouseEvent e source-viewer]
  (let [click-count (.getClickCount e)
        cf (click-fn click-count)
        pos (caret source-viewer)]
    (remember-caret-col source-viewer pos)
    (clear-snippet-tab-triggers! source-viewer)
    (when cf
      (handler-run
        (:command cf)
        [(handler-context source-viewer)]
        e)
      (.consume e)
      (last-command! source-viewer (:command cf)))))

(defn- adjust-replace-length [doc pos n]
  (if (> (+ pos n) (count doc))
    0
    n))

(defn- replace-text-selection [source-viewer s]
  (let [np (selection-offset source-viewer)
        sel-len (selection-length source-viewer)]
    (text-selection! source-viewer 0 0)
    (replace! source-viewer np sel-len s)
    (caret! source-viewer (+ np (count s)) false)
    (show-line source-viewer)
    (remember-caret-col source-viewer (+ np (count s)))))

(defn- replace-text-and-caret [source-viewer offset length s new-caret-pos]
  (let [doc (text source-viewer)
        pos (adjust-bounds doc offset)
        new-len (adjust-replace-length doc pos length)
        new-pos (adjust-bounds doc new-caret-pos)]
    (replace! source-viewer pos new-len s)
    (caret! source-viewer (adjust-bounds (text source-viewer) new-caret-pos) false)
    (show-line source-viewer)
    (remember-caret-col source-viewer new-caret-pos)))

(handler/defhandler :copy :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer clipboard]
    (text! clipboard (text-selection source-viewer))))

(handler/defhandler :paste :code-view
  (enabled? [source-viewer] (editable? source-viewer))
  (run [source-viewer clipboard]
    (when-let [clipboard-text (text clipboard)]
      (let [clipboard-text (text-util/crlf->lf clipboard-text)
            caret (caret source-viewer)]
        (if (pos? (selection-length source-viewer))
          (replace-text-selection source-viewer clipboard-text)
          (replace-text-and-caret source-viewer caret 0 clipboard-text (+ caret (count clipboard-text))))
        (typing-changes! source-viewer)))))

(handler/defhandler :cut :code-view
  (enabled? [source-viewer] (and (editable? source-viewer) (pos? (selection-length source-viewer))))
  (run [source-viewer clipboard]
       (text! clipboard (text-selection source-viewer))
       (replace-text-selection source-viewer "")
       (typing-changes! source-viewer)))

(defn- caret-at-left-of-selection? [source-viewer]
  (= (caret source-viewer)
     (selection-offset source-viewer)))

(defn right [source-viewer]
  (let [c (caret source-viewer)
        doc (text source-viewer)
        selected-text (text-selection source-viewer)
        next-pos (if (and (pos? (count selected-text))
                          (caret-at-left-of-selection? source-viewer))
                   (adjust-bounds doc (+ c (count selected-text)))
                   (adjust-bounds doc (inc c)))]
    (clear-snippet-tab-triggers! source-viewer)
    (caret! source-viewer next-pos false)
    (remember-caret-col source-viewer next-pos)))

(defn select-right [source-viewer]
  (let [c (caret source-viewer)
        doc (text source-viewer)
        selected-text (text-selection source-viewer)
        next-pos (adjust-bounds doc (inc c))]
    (clear-snippet-tab-triggers! source-viewer)
    (caret! source-viewer next-pos true)
    (remember-caret-col source-viewer next-pos)))

(defn left [source-viewer]
  (let [c (caret source-viewer)
        doc (text source-viewer)
        selected-text (text-selection source-viewer)
        next-pos (if (and (pos? (count selected-text))
                          (not (caret-at-left-of-selection? source-viewer)))
                   (adjust-bounds doc (- c (count selected-text)))
                   (adjust-bounds doc (dec c)))]
    (clear-snippet-tab-triggers! source-viewer)
    (caret! source-viewer next-pos false)
    (remember-caret-col source-viewer next-pos)))

(defn select-left [source-viewer]
  (let [c (caret source-viewer)
        doc (text source-viewer)
        selected-text (text-selection source-viewer)
        next-pos (adjust-bounds doc (dec c))]
    (clear-snippet-tab-triggers! source-viewer)
    (caret! source-viewer next-pos true)
    (remember-caret-col source-viewer next-pos)))

(handler/defhandler :right :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (right source-viewer)
    (state-changes! source-viewer)))

(handler/defhandler :left :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer user-data]
    (left source-viewer)
    (state-changes! source-viewer)))

(handler/defhandler :select-right :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (select-right source-viewer)
    (state-changes! source-viewer)))

(handler/defhandler :select-left :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (select-left source-viewer)
    (state-changes! source-viewer)))

(defn- effective-column [line column]
  (loop [remaining column
         tab-adj-col 0
         line (seq line)]
    (if-let [next-char (first line)]
      (let [next-char-size (if (= next-char \tab) tab-size 1)]
        (if (> next-char-size remaining)
          tab-adj-col
          (recur (- remaining next-char-size)
                 (inc tab-adj-col)
                 (rest line))))
      tab-adj-col)))

(defn up-line [source-viewer preferred-offset]
  (let [line-before-num (prev-line-num source-viewer)]
    (when-not (neg? line-before-num)
      (let [line-before (prev-line source-viewer)
            line-before-offset (line-offset-at-num source-viewer line-before-num)
            target-column (effective-column line-before preferred-offset)]
        (+ line-before-offset target-column)))))

(defn down-line [source-viewer preferred-offset]
  (when-let [line-after-num (next-line-num source-viewer)]
    (let [line-after (next-line source-viewer)
          line-after-offset (line-offset-at-num source-viewer line-after-num)
          target-column (effective-column line-after preferred-offset)]
      (adjust-bounds (text source-viewer) (+ line-after-offset target-column)))))

(defn up [source-viewer show?]
  (let [doc (text source-viewer)
        preferred-offset (preferred-offset source-viewer)
        next-pos (if (pos? (selection-length source-viewer))
                   (adjust-bounds doc (selection-offset source-viewer))
                   (or (up-line source-viewer preferred-offset) (caret source-viewer)))]
    (caret! source-viewer next-pos false)
    (when show? (show-line source-viewer))))

(defn select-up [source-viewer show?]
  (let [doc (text source-viewer)
        preferred-offset (preferred-offset source-viewer)
        next-pos (or (up-line source-viewer preferred-offset) 0)]
    (when (not= next-pos (caret source-viewer))
      (caret! source-viewer next-pos true))
    (when show? (show-line source-viewer))))

(defn down [source-viewer show?]
  (let [doc (text source-viewer)
        preferred-offset (preferred-offset source-viewer)
        next-pos (if (pos? (selection-length source-viewer))
                   (adjust-bounds doc (+ (selection-offset source-viewer)
                                         (selection-length source-viewer)))
                   (or (down-line source-viewer preferred-offset) (caret source-viewer)))]
      (caret! source-viewer next-pos false)
      (when show? (show-line source-viewer))))

(defn select-down [source-viewer show?]
  (let [doc (text source-viewer)
        preferred-offset (preferred-offset source-viewer)
        next-pos (or (down-line source-viewer preferred-offset) (count doc))]
    (when (not= next-pos (caret source-viewer))
      (caret! source-viewer next-pos true))
    (when show? (show-line source-viewer))))

(handler/defhandler :up :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (up source-viewer true)
    (state-changes! source-viewer)))

(handler/defhandler :down :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer user-data]
    (down source-viewer true)
    (state-changes! source-viewer)))

(handler/defhandler :select-up :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (select-up source-viewer true)
    (state-changes! source-viewer)))

(handler/defhandler :select-down :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer user-data]
    (select-down source-viewer true)
    (state-changes! source-viewer)))

(defn do-page-up [source-viewer with-select?]
  (let [first-line (first-visible-row-number source-viewer)
        last-line (last-visible-row-number source-viewer)]
    (page-up source-viewer)
    (dotimes [n (- last-line first-line)]
      (if with-select?
        (select-up source-viewer false)
        (up source-viewer false)))))

(defn do-page-down [source-viewer with-select?]
    (let [first-line (first-visible-row-number source-viewer)
          last-line (last-visible-row-number source-viewer)]
      (page-down source-viewer)
      (dotimes [n (- last-line first-line)]
       (if with-select?
         (select-down source-viewer false)
         (down source-viewer false)))))

(handler/defhandler :page-up :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (do-page-up source-viewer false)
    (state-changes! source-viewer)))

(handler/defhandler :page-down :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (do-page-down source-viewer false)
    (state-changes! source-viewer)))

(handler/defhandler :select-page-up :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (do-page-up source-viewer true)
    (state-changes! source-viewer)))

(handler/defhandler :select-page-down :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (do-page-down source-viewer true)
    (state-changes! source-viewer)))

(def word-regex  #"\n|\s*_*[a-zA-Z0-9\=\+\-\*\!]+|.")

(defn next-word-move [doc np]
  (re-find word-regex (subs doc np)))

(defn next-word [source-viewer select?]
  (let [np (caret source-viewer)
        doc (text source-viewer)
        next-word-move (next-word-move doc np)
        next-pos (+ np (count next-word-move))]
    (caret! source-viewer next-pos select?)
    (show-line source-viewer)
    (remember-caret-col source-viewer next-pos)))

(defn prev-word-move [doc np]
  (re-find word-regex (->> (subs doc 0 np) (reverse) (apply str))))

(defn prev-word [source-viewer select?]
  (let [np (caret source-viewer)
        doc (text source-viewer)
        next-word-move (prev-word-move doc np)
        next-pos (- np (count next-word-move))]
    (caret! source-viewer next-pos select?)
    (show-line source-viewer)
    (remember-caret-col source-viewer next-pos)))

(handler/defhandler :next-word :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (next-word source-viewer false)
    (state-changes! source-viewer)))

(handler/defhandler :prev-word :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer user-data]
    (prev-word source-viewer false)
    (state-changes! source-viewer)))

(handler/defhandler :select-next-word :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (next-word source-viewer true)
    (state-changes! source-viewer)))

(handler/defhandler :select-prev-word :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer user-data]
    (prev-word source-viewer true)
    (state-changes! source-viewer)))

(defn- get-indentation [line]
  (re-find #"^[\t ]*" line))

(defn beginning-of-line-text-pos [source-viewer]
  (let [line-offset (line-offset source-viewer)
        line-indentation-len (count (get-indentation (line source-viewer)))]
    (+ line-offset line-indentation-len)))

(defn beginning-of-line [source-viewer select?]
  (let [next-pos (line-offset source-viewer)]
    (caret! source-viewer next-pos select?)
    (remember-caret-col source-viewer next-pos)))

(defn beginning-of-line-text [source-viewer select?]
  (let [beginning-of-line-text-pos (beginning-of-line-text-pos source-viewer)
        next-pos (if (= beginning-of-line-text-pos (caret source-viewer))
                   (line-offset source-viewer)
                   beginning-of-line-text-pos)]
    (caret! source-viewer next-pos select?)
    (remember-caret-col source-viewer next-pos)))

(defn end-of-line-pos [source-viewer]
  (let [line-text-len (count (line source-viewer))
        line-offset (line-offset source-viewer)]
    (+ line-offset line-text-len)))

(defn end-of-line [source-viewer select?]
  (let [next-pos (end-of-line-pos source-viewer)]
    (caret! source-viewer next-pos select?)
    (remember-caret-col source-viewer next-pos)))

(handler/defhandler :beginning-of-line :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (beginning-of-line source-viewer false)
    (state-changes! source-viewer)))

(handler/defhandler :beginning-of-line-text :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (beginning-of-line-text source-viewer false)
    (state-changes! source-viewer)))

(handler/defhandler :end-of-line :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (end-of-line source-viewer false)
    (state-changes! source-viewer)))

(handler/defhandler :select-beginning-of-line :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (beginning-of-line source-viewer true)
    (state-changes! source-viewer)))

(handler/defhandler :select-beginning-of-line-text :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (beginning-of-line-text source-viewer true)
    (state-changes! source-viewer)))

(handler/defhandler :select-end-of-line :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (end-of-line source-viewer true)
    (state-changes! source-viewer)))

(handler/defhandler :beginning-of-file :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (caret! source-viewer 0 false)
    (show-line source-viewer)
    (remember-caret-col source-viewer 0)
    (state-changes! source-viewer)))

(handler/defhandler :end-of-file :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (let [doc (text source-viewer)]
      (caret! source-viewer (count doc) false)
      (show-line source-viewer)
      (remember-caret-col source-viewer (caret source-viewer))
      (state-changes! source-viewer))))

(handler/defhandler :select-beginning-of-file :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (caret! source-viewer 0 true)
    (show-line source-viewer)
    (remember-caret-col source-viewer 0)
    (state-changes! source-viewer)))

(handler/defhandler :select-end-of-file :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer user-data]
    (let [doc (text source-viewer)]
      (caret! source-viewer (count doc) true)
      (show-line source-viewer)
      (remember-caret-col source-viewer (caret source-viewer))
      (state-changes! source-viewer))))

(defn go-to-line [source-viewer line]
  (when line
    (let [line-count (line-count source-viewer)
          line-num (if (> 1 line) 0 (dec line))
          np (if (>= line-count line-num)
               (line-offset-at-num source-viewer line-num)
               (count (text source-viewer)))]
      (caret! source-viewer np false)
      (remember-caret-col source-viewer np)
      (show-line source-viewer))))

(handler/defhandler :goto-line :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    ;; when using show and wait on the dialog, was getting very
    ;; strange double events not solved by consume - this is a
    ;; workaround to let us use show
    (let [line-number (promise)
          ^Stage stage (dialogs/make-goto-line-dialog line-number)
          go-to-line-fn (fn [] (when (realized? line-number)
                                 (go-to-line source-viewer @line-number)
                                 (state-changes! source-viewer)))]
      (.setOnHidden stage (ui/event-handler e (go-to-line-fn))))))

(handler/defhandler :select-word :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (let [regex #"^\w+"
          np (caret source-viewer)
          doc (text source-viewer)
          word-end (re-find regex (subs doc np))
          text-before (->> (subs doc 0 (adjust-bounds doc (inc np))) (reverse) (rest) (apply str))
          word-begin (re-find regex text-before)
          start-pos (- np (count word-begin))
          end-pos (+ np (count word-end))
          len (- end-pos start-pos)]
      (text-selection! source-viewer start-pos len)
      (state-changes! source-viewer))))

(handler/defhandler :select-line :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (let [regex #"^\w+"
          line (line source-viewer)
          line-offset (line-offset source-viewer)]
      (text-selection! source-viewer line-offset (count line))
      (state-changes! source-viewer))))

(handler/defhandler :select-all :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (caret! source-viewer (count (text source-viewer)) false)
    (text-selection! source-viewer 0 (count (text source-viewer)))
    (remember-caret-col source-viewer (caret source-viewer))
    (state-changes! source-viewer)))

(defn delete [source-viewer forward?]
  (let [np (caret source-viewer)
        doc (text source-viewer)
        slen (selection-length source-viewer)
        soffset (selection-offset source-viewer)]
    (if (pos? slen)
      (replace-text-and-caret source-viewer soffset slen "" soffset)
      (let [pos (if forward? np (adjust-bounds doc (dec np)))
            target (subs doc pos (adjust-bounds doc (+ 2 pos)))]
       (when-not (and (zero? np) (not forward?))
         (if (contains? auto-delete-set target)
           (replace-text-and-caret source-viewer pos 2 "" pos)
           (replace-text-and-caret source-viewer pos 1 "" pos)))))))

(handler/defhandler :delete-backward :code-view
  (enabled? [source-viewer] (editable? source-viewer))
  (run [source-viewer]
    (when (editable? source-viewer)
      (delete source-viewer false)
      (typing-changes! source-viewer true))))

(handler/defhandler :delete :code-view
  (enabled? [source-viewer] (editable? source-viewer))
  (run [source-viewer]
    (when (editable? source-viewer)
      (delete source-viewer true)
      (typing-changes! source-viewer true))))

(defn delete-line [source-viewer]
  (let [np (caret source-viewer)
        doc (text source-viewer)
        line-begin-offset (line-offset source-viewer)
        line-end-offset (end-of-line-pos source-viewer)
        consume-pos (if (= line-end-offset (count doc))
                      line-end-offset
                      (inc line-end-offset))
        new-doc (str (subs doc 0 line-begin-offset)
                     (subs doc consume-pos))]
    (text! source-viewer new-doc)
    (caret! source-viewer (adjust-bounds new-doc line-begin-offset) false)
    (remember-caret-col source-viewer (caret source-viewer))
    (show-line source-viewer)))

(handler/defhandler :delete-line :code-view
  (enabled? [source-viewer] (editable? source-viewer))
  (run [source-viewer]
    (delete-line source-viewer)
    (typing-changes! source-viewer)))

(handler/defhandler :delete-prev-word :code-view
  (enabled? [source-viewer] (editable? source-viewer))
  (run [source-viewer]
    (let [np (caret source-viewer)
          doc (text source-viewer)
          next-word-move (prev-word-move doc np)
          new-pos (- np (count next-word-move))
          new-doc (str (subs doc 0 new-pos)
                       (subs doc np))]
      (text! source-viewer new-doc)
      (caret! source-viewer new-pos false)
      (remember-caret-col source-viewer (caret source-viewer))
      (show-line source-viewer))
    (typing-changes! source-viewer)))

(handler/defhandler :delete-next-word :code-view
  (enabled? [source-viewer] (editable? source-viewer))
  (run [source-viewer]
    (let [np (caret source-viewer)
          doc (text source-viewer)
          next-word-move (next-word-move doc np)
          next-word-pos (+ np (count next-word-move))
          new-doc (str (subs doc 0 np)
                       (subs doc next-word-pos))]
      (text! source-viewer new-doc))
    (typing-changes! source-viewer)))

(handler/defhandler :delete-to-end-of-line :code-view
  (enabled? [source-viewer] (editable? source-viewer))
  (run [source-viewer]
    (let [np (caret source-viewer)
          doc (text source-viewer)
          line-end-offset (end-of-line-pos source-viewer)
          new-doc (str (subs doc 0 np)
                       (subs doc line-end-offset))]
      (text! source-viewer new-doc))
    (typing-changes! source-viewer)))

(handler/defhandler :cut-to-end-of-line :code-view
  (enabled? [source-viewer] (editable? source-viewer))
  (run [source-viewer clipboard]
    (let [np (caret source-viewer)
          doc (text source-viewer)
          line-end-offset (end-of-line-pos source-viewer)
          consume-pos (if (= line-end-offset np) (adjust-bounds doc (inc line-end-offset)) line-end-offset)
          new-doc (str (subs doc 0 np)
                       (subs doc consume-pos))
          clipboard-text (if (= :cut-to-end-of-line (last-command source-viewer))
                           (str (text clipboard) (subs doc np consume-pos))
                           (subs doc np consume-pos))]
      (text! clipboard clipboard-text)
      (text! source-viewer new-doc))
    (typing-changes! source-viewer)))

(handler/defhandler :delete-to-beginning-of-line :code-view
  (enabled? [source-viewer] (editable? source-viewer))
  (run [source-viewer]
    (let [np (caret source-viewer)
          doc (text source-viewer)
          line-begin-offset (line-offset source-viewer)
          new-doc (str (subs doc 0 line-begin-offset)
                       (subs doc np))]
      (text! source-viewer new-doc)
      (caret! source-viewer line-begin-offset false)
      (remember-caret-col source-viewer line-begin-offset))
    (typing-changes! source-viewer)))

(defn- select-found-text [source-viewer doc found-idx tlen caret-side]
  (when (<= 0 found-idx)
    (let [caret-pos (if (= caret-side :caret-after) (adjust-bounds doc (+ found-idx tlen)) found-idx)]
      (caret! source-viewer caret-pos false)
      (show-line source-viewer)
      (text-selection! source-viewer found-idx tlen)
      (remember-caret-col source-viewer (caret source-viewer)))))

(defn- matcher-hit-range [^Matcher matcher]
  {:start (.start matcher) :length (- (.end matcher) (.start matcher))})

(defn- selected-range [source-viewer]
  {:start (selection-offset source-viewer) :length (selection-length source-viewer)})

(defn- range-start ^long [{:keys [start]}]
  start)

(defn- range-end ^long [{:keys [start length]}]
  (+ start length))

(defn- range-length ^long [{:keys [length]}]
  length)

(defn- do-find-next [find-params doc from to]
  (when-let [find-expr (create-find-expr find-params)]
    (let [pattern (re-pattern find-expr)
          matcher (re-matcher pattern (subs doc from to))]
      (if (re-find matcher)
        (update (matcher-hit-range matcher) :start + from)))))

(defn find-next [source-viewer]
  (let [doc (text source-viewer)
        find-params (get-find-params)]
    (when-let [hit (or (do-find-next find-params doc (caret source-viewer) (count doc))
                       (and (:wrap find-params) (do-find-next find-params doc 0 (count doc))))]
      (select-found-text source-viewer doc (range-start hit) (range-length hit) :caret-after)
      (state-changes! source-viewer))))

(handler/defhandler :find-next :find-bar
  (run [source-viewer]
    (find-next source-viewer)))

(handler/defhandler :find-next :replace-bar
  (run [source-viewer]
    (find-next source-viewer)))

(handler/defhandler :find-next :code-view
  (run [source-viewer]
    (find-next source-viewer)))

(defn- find-last-hit-before [matcher pos]
  (loop [found (re-find matcher)
         last-hit nil]
    (if (not found)
      last-hit
      (let [hit (matcher-hit-range matcher)]
        (if (< (range-start hit) pos)
          (recur (re-find matcher) hit)
          last-hit)))))

(defn- do-find-prev [find-params doc from to]
  (when-let [find-expr (create-find-expr find-params)]
    (let [pattern (re-pattern find-expr)
          matcher (re-matcher pattern doc)]
      (find-last-hit-before matcher to))))

(defn find-prev [source-viewer]
  (let [doc (text source-viewer)
        find-params (get-find-params)]
    (when-let [hit (or (do-find-prev find-params doc 0 (caret source-viewer))
                       (and (:wrap find-params) (do-find-prev find-params doc (caret source-viewer) (count doc))))]
      (select-found-text source-viewer doc (range-start hit) (range-length hit) :caret-before)
      (state-changes! source-viewer))))

(handler/defhandler :find-prev :find-bar
  (run [source-viewer]
    (find-prev source-viewer)))

(handler/defhandler :find-prev :replace-bar
  (run [source-viewer]
    (find-prev source-viewer)))

(handler/defhandler :find-prev :code-view
  (run [source-viewer]
    (find-prev source-viewer)))

(defn- subs-range [text {:keys [start length]}]
  (subs text start (+ start length)))

(defn- do-check-replace-range [doc find-params selected-range]
  (when-let [find-expr (create-find-expr find-params)]
    (let [pattern (re-pattern find-expr)
          selected-text (subs-range doc selected-range)]
      (when (re-matches pattern selected-text)
        selected-range))))
              
(defn replace-next [source-viewer]
  (let [doc (text source-viewer)
        find-params (get-find-params)]
    (when-let [replace-range (do-check-replace-range doc find-params (selected-range source-viewer))]
      (replace! source-viewer (range-start replace-range) (range-length replace-range) (:replacement find-params))
      (caret! source-viewer (+ (range-start replace-range) (count (:replacement find-params))) false)
      (show-line source-viewer)
      (state-changes! source-viewer)
      (remember-caret-col source-viewer (caret source-viewer)))
    (find-next source-viewer)))

(handler/defhandler :replace-next :replace-bar
  (enabled? [source-viewer] (editable? source-viewer))
  (run [source-viewer]
    (replace-next source-viewer)))

(handler/defhandler :replace-next :code-view
  (run [source-viewer]
    (replace-next source-viewer)))

(defn- do-replace-all [doc find-params caret-pos]
  (let [replacement (:replacement find-params)]
    (loop [doc doc
           next-find-start 0
           caret-pos caret-pos
           hit (do-find-next find-params doc next-find-start (count doc))]
      (if hit
        (let [doc (str (subs doc 0 (range-start hit)) replacement (subs doc (range-end hit)))
              next-find-start (+ (range-start hit) (count replacement))
              caret-pos (if (< (range-start hit) caret-pos) (+ caret-pos (count replacement)) caret-pos)]
          (recur doc next-find-start caret-pos (do-find-next find-params doc next-find-start (count doc))))
        {:doc doc :caret caret-pos}))))

(defn replace-all [source-viewer]
  (let [doc (text source-viewer)
        find-params (get-find-params)
        caret-pos (caret source-viewer)
        result (do-replace-all doc find-params caret-pos)]
    (text! source-viewer (:doc result))
    (caret! source-viewer (:caret result) false)
    (show-line source-viewer)
    (state-changes! source-viewer)
    (remember-caret-col source-viewer (caret source-viewer))))

(handler/defhandler :replace-all :replace-bar
  (enabled? [source-viewer] (editable? source-viewer))
  (run [source-viewer]
    (replace-all source-viewer)))

(defn commented? [syntax s] (string/starts-with? s (:line-comment syntax)))

(defn add-comment [source-viewer text line-comment]
  (str line-comment text))

(defn remove-comment [source-viewer text line-comment]
  (subs text (count line-comment)))

(defn- syntax [source-viewer]
  (ui/user-data source-viewer :editor.code-view/syntax))

(defn alter-line [source-viewer line-num f & args]
  (let [current-line (line-at-num source-viewer line-num)
        line-offset (line-offset-at-num source-viewer line-num)
        new-text (apply f source-viewer current-line args)]
    (when new-text
      (replace! source-viewer (adjust-bounds (text source-viewer) line-offset) (count current-line) new-text))))

(defn comment-line [source-viewer line-comment line-num]
  (alter-line source-viewer line-num add-comment line-comment))

(defn uncomment-line [source-viewer line-comment line-num]
  (alter-line source-viewer line-num remove-comment line-comment))

(defn toggle-line-comment [source-viewer]
  (let [syntax (syntax source-viewer)
        line-comment (:line-comment syntax)
        caret-offset (caret source-viewer)
        caret-line-num (line-num-at-offset source-viewer caret-offset)
        comment-present? (commented? syntax (line-at-num source-viewer caret-line-num))
        op (if comment-present? uncomment-line comment-line)
        caret-delta (* (count line-comment) (if comment-present? -1 1))]
    (op source-viewer line-comment caret-line-num)
    (caret! source-viewer (+ caret-offset caret-delta) false)
    (remember-caret-col source-viewer (caret source-viewer))))

(defn comment-line-syntax
  [source-viewer]
  (:line-comment (syntax source-viewer)))

(defn comment-region [source-viewer]
  (let [line-comment (comment-line-syntax source-viewer)
        region-start (selection-offset source-viewer)
        region-len (selection-length source-viewer)
        region-end (+ region-start region-len)
        start-line-num (line-num-at-offset source-viewer region-start)
        start-line-offset (line-offset-at-num source-viewer start-line-num)
        end-line-num (line-num-at-offset source-viewer region-end)
        end-line-offset (line-offset-at-num source-viewer end-line-num)
        caret-offset (caret source-viewer)
        caret-line-num (line-num-at-offset source-viewer caret-offset)]
    (doseq [i (range start-line-num (inc end-line-num))]
      (comment-line source-viewer line-comment i))
    (let [line-comment-len (count line-comment)
          caret-offset-delta (* line-comment-len (inc (- caret-line-num start-line-num)))]
     (caret! source-viewer (+ caret-offset-delta caret-offset) false)
     (text-selection! source-viewer (+ line-comment-len region-start)
                      (+ region-len (* line-comment-len (- end-line-num start-line-num))))
     (remember-caret-col source-viewer (caret source-viewer)))))

(defn uncomment-region [source-viewer]
  (let [line-comment (comment-line-syntax source-viewer)
        region-start (selection-offset source-viewer)
        region-len (selection-length source-viewer)
        region-end (+ region-start region-len)
        start-line-num (line-num-at-offset source-viewer region-start)
        start-line-offset (line-offset-at-num source-viewer start-line-num)
        end-line-num (line-num-at-offset source-viewer region-end)
        end-line-offset (line-offset-at-num source-viewer end-line-num)
        caret-offset (caret source-viewer)
        caret-line-num (line-num-at-offset source-viewer caret-offset)]
    (doseq [i (range start-line-num (inc end-line-num))]
      (uncomment-line source-viewer line-comment i))
    (let [line-comment-len (count line-comment)
          caret-offset-delta (* line-comment-len (inc (- caret-line-num start-line-num)))]
      (caret! source-viewer (- caret-offset caret-offset-delta) false)
      (text-selection! source-viewer (- region-start line-comment-len)
                       (- region-len (* line-comment-len (- end-line-num start-line-num))))
      (remember-caret-col source-viewer (caret source-viewer)))))

(defn selected-lines [source-viewer]
  (let [region-start (selection-offset source-viewer)
        region-end (+ region-start (selection-length source-viewer))
        start-line-num (line-num-at-offset source-viewer region-start)
        end-line-num (line-num-at-offset source-viewer region-end)]
    (for [line-num (range start-line-num (inc end-line-num))]
      (line-at-num source-viewer line-num))))

(handler/defhandler :toggle-comment :code-view
  (enabled? [source-viewer] (and (editable? source-viewer) (comment-line-syntax source-viewer)))
  (run [source-viewer]
    (if (pos? (count (text-selection source-viewer)))
      (if (every? #(commented? (syntax source-viewer) %) (selected-lines source-viewer))
        (uncomment-region source-viewer)
        (comment-region source-viewer))
      (toggle-line-comment source-viewer))
    (typing-changes! source-viewer)))

(defn enter-key-text [source-viewer key-typed]
  (let [np (caret source-viewer)
        doc (text source-viewer)]
    (if (pos? (selection-length source-viewer))
      (replace-text-selection source-viewer key-typed)
      (replace-text-and-caret source-viewer np 0 key-typed (+ np (count key-typed))))))

(defn next-tab-trigger [source-viewer pos]
  (when (has-snippet-tab-trigger? source-viewer)
    (let [doc (text source-viewer)
          tab-trigger-info (next-snippet-tab-trigger! source-viewer)
          search-text (:trigger tab-trigger-info)
          exit-text (:exit tab-trigger-info)]
      (if (= :end search-text)
        (do
          (caret! source-viewer pos false)
          (if exit-text
            (let [found-idx (string/index-of doc exit-text pos)
                  tlen (count exit-text)]
              (when found-idx
                (caret! source-viewer (+ found-idx tlen) false)
                (show-line source-viewer)
                (remember-caret-col source-viewer (caret source-viewer)))
              (right source-viewer))))
        (let [found-idx (string/index-of doc search-text pos)
              tlen (count search-text)]
          (when found-idx
            (select-found-text source-viewer doc found-idx tlen :caret-after)))))))

(defn prev-tab-trigger [source-viewer pos]
  (let [doc (text source-viewer)
        tab-trigger-info (prev-snippet-tab-trigger! source-viewer)
        search-text (:trigger tab-trigger-info)]
    (when-not (= :begin search-text)
      (let [found-idx (string/last-index-of doc search-text pos)
            tlen (count search-text)]
        (when found-idx
          (select-found-text source-viewer doc found-idx tlen :caret-after))))))

(handler/defhandler :backwards-tab-trigger :code-view
  (enabled? [source-viewer] (editable? source-viewer))
  (run [source-viewer]
    (when (editable? source-viewer)
      (if (has-prev-snippet-tab-trigger? source-viewer)
        (let [np (caret source-viewer)
              doc (text source-viewer)]
          (prev-tab-trigger source-viewer np)
          (state-changes! source-viewer))))))

(defn- trim-indentation [line]
  (second (re-find #"^[\t ]*(.*)" line)))

(defn- untabify [whitespace]
  (string/replace whitespace #"\t" "    "))

(defn- split-indentation [indentation]
  (let [[_ indents remains] (re-find #"^((?:    |\t)*)([\t ]*)" indentation)]
    (if (< (count (untabify remains)) tab-size)
      [indents remains]
      (split-indentation (untabify indentation)))))

(defn- increase-indents [indent-split indentation]
  (update indent-split 0 str indentation))

(defn- decrease-indents [indent-split]
  (update indent-split 0 string/replace #"(    |\t)$" ""))

(defn- make-indented-line [indent-split line]
  (str (first indent-split) (second indent-split) (trim-indentation line)))

(defn- keep-indentation [line line-above]
  (-> (get-indentation line-above)
      (split-indentation)
      (make-indented-line line)))

(defn indent-line [source-viewer line line-above]
  (when-let [indentation-data (:indentation (syntax source-viewer))]
    (let [{:keys [increase? decrease?]} indentation-data]
      (cond
        ;; function asdf()
        ;; end|
        (and (decrease? line) (increase? line-above))
        (keep-indentation line line-above)

        ;; if foo
        ;;     ...
        ;;     ...
        ;;     end|
        (decrease? line)
        (-> (get-indentation line-above)
            (split-indentation)
            (decrease-indents)
            (make-indented-line line))

        ;; function foo()
        ;; |
        (increase? line-above)
        (-> (get-indentation line-above)
            (split-indentation)
            (increase-indents default-indentation)
            (make-indented-line line))

        ;; function foo()
        ;;     ...
        ;;     ...
        ;; |
        :else
        (keep-indentation line line-above)))))

(defn unindent-line [source-viewer line line-above]
  (when-let [indentation-data (:indentation (syntax source-viewer))]
    (let [{:keys [increase? decrease?]} indentation-data]
      (cond
        ;; if foo
        ;;     ...
        ;;     end|
        (and (decrease? line) (not (increase? line-above)))
        (-> (get-indentation line-above)
            (split-indentation)
            (decrease-indents)
            (make-indented-line line))

        ;; if foo
        ;;     end|
        (decrease? line)
        (keep-indentation line line-above)

        :else
        line))))

(defn do-unindent-line [source-viewer line-num]
  (let [current-line (line-at-num source-viewer line-num)
        line-offset (line-offset-at-num source-viewer line-num)
        caret-offset (caret source-viewer)
        prev-line (if (= 0 line-num) "" (line-at-num source-viewer (dec line-num)))
        unindent-text (unindent-line source-viewer current-line prev-line)
        caret-delta (- (count unindent-text) (count current-line))]
    (if unindent-text
      (do
        (replace! source-viewer (adjust-bounds (text source-viewer) line-offset) (count current-line) unindent-text)
        (caret! source-viewer (+ caret-offset caret-delta) false)
        (remember-caret-col source-viewer (caret source-viewer))
        caret-delta)
      0)))

(defn do-indent-line [source-viewer line-num]
  (let [current-line (line-at-num source-viewer line-num)
        line-offset (line-offset-at-num source-viewer line-num)
        caret-offset (caret source-viewer)
        prev-line (if (= 0 line-num) "" (line-at-num source-viewer (dec line-num)))
        indent-text (indent-line source-viewer current-line prev-line)
        caret-delta (- (count indent-text) (count current-line))]
    (if indent-text
      (do
        (replace! source-viewer (adjust-bounds (text source-viewer) line-offset) (count current-line) indent-text)
        (caret! source-viewer (+ caret-offset caret-delta) false)
        (remember-caret-col source-viewer (caret source-viewer))
        caret-delta)
      0)))

(defn do-indent-region [source-viewer region-start region-end]
  (let [region-start (selection-offset source-viewer)
        region-len   (selection-length source-viewer)
        caret-offset (caret source-viewer)
        start-line-num (line-num-at-offset source-viewer region-start)
        start-line-offset (line-offset-at-num source-viewer start-line-num)
        end-line-num (line-num-at-offset source-viewer region-end)
        end-line-offset (line-offset-at-num source-viewer end-line-num)]
    (loop [i           start-line-num
           caret-delta 0]
      (if (<= i end-line-num)
        (recur (inc i) (int (+ caret-delta (do-indent-line source-viewer i))))
        (if (= caret-offset region-start)
          (do (caret! source-viewer (+ region-start region-len caret-delta) false)
              (caret! source-viewer caret-offset true))
          (do (caret! source-viewer region-start false)
              (caret! source-viewer (+ caret-offset caret-delta) true))))))
  (remember-caret-col source-viewer (caret source-viewer)))

(handler/defhandler :reindent :code-view
  (enabled? [source-viewer] (editable? source-viewer))
  (run [source-viewer]
    (when (editable? source-viewer)
      (if (pos? (count (text-selection source-viewer)))
        (let [region-start (selection-offset source-viewer)
              region-len (selection-length source-viewer)
              region-end (+ region-start region-len)]
          (do-indent-region source-viewer region-start region-end))
        (let [line-num (line-num-at-offset source-viewer (caret source-viewer))]
          (do-indent-line source-viewer line-num)))
      (typing-changes! source-viewer))))

(handler/defhandler :tab :code-view
  (enabled? [source-viewer] (editable? source-viewer))
  (run [source-viewer]
    (when (editable? source-viewer)
      (if (has-snippet-tab-trigger? source-viewer)
        (let [np (caret source-viewer)
              doc (text source-viewer)]
          (next-tab-trigger source-viewer np)
          (typing-changes! source-viewer))
        (if (pos? (selection-length source-viewer))
          (let [region-start (selection-offset source-viewer)
                region-len (selection-length source-viewer)
                region-end (+ region-start region-len)]
            (do-indent-region source-viewer region-start region-end))
          (do
            (enter-key-text source-viewer "\t")
            (typing-changes! source-viewer)))))))

(handler/defhandler :enter :code-view
  (enabled? [source-viewer] (editable? source-viewer))
  (run [source-viewer]
    ;; e(fx)clipse doesn't like \r\n
    ;; should really be (System/getProperty "line.separator")
    (let [line-separator "\n"]
      (when (editable? source-viewer)
        (clear-snippet-tab-triggers! source-viewer)
        (if (= (caret source-viewer) (end-of-line-pos source-viewer))
          (do
            (do-unindent-line source-viewer (line-num-at-offset source-viewer (caret source-viewer)))
            (enter-key-text source-viewer line-separator)
            (do-indent-line source-viewer (line-num-at-offset source-viewer (caret source-viewer)))
            (caret! source-viewer (+ (line-offset source-viewer) (count (line source-viewer))) false)
            (remember-caret-col source-viewer (caret source-viewer))
            (show-line source-viewer))
          (enter-key-text source-viewer line-separator))
        (typing-changes! source-viewer)))))

(defn- completion-pattern [replacement loffset line-text offset]
  (let [fragment (subs line-text 0 (- offset loffset))
        length-to-first-whitespace (string/index-of (->> fragment reverse (apply str)) " ")
        start (if length-to-first-whitespace (- (count fragment) length-to-first-whitespace 1) 0)
        completion-text (subs line-text (max start 0) (max (- offset loffset) 0))
        parsed-line (code/parse-line completion-text)
        replacement-in-line? (when replacement (string/index-of fragment replacement start))]
    (if replacement-in-line?
      replacement
      (code/proposal-filter-pattern (:namespace parsed-line) (:function parsed-line)))))

(defn do-proposal-replacement [source-viewer replacement]
  (let [tab-triggers (:tab-triggers replacement)
        replacement (:insert-string replacement)
        offset (caret source-viewer)
        line-text (line source-viewer)
        loffset (line-offset source-viewer)
        pattern (completion-pattern replacement loffset line-text offset)
        search-start (adjust-bounds (text source-viewer) (- offset loffset (count pattern)))
        replacement-start (string/index-of line-text pattern search-start)
        replacement-len (count pattern)]
    (when replacement-start
      (replace! source-viewer (+ loffset replacement-start) replacement-len replacement)
      (do-indent-region source-viewer loffset (+ loffset (count replacement)))
      (snippet-tab-triggers! source-viewer tab-triggers)
      (if (has-snippet-tab-trigger? source-viewer)
        (let [nl (line source-viewer)
              snippet-tab-start (or (when-let [start (:start tab-triggers)]
                                      (string/index-of nl start replacement-start))
                                    (string/index-of nl "(" replacement-start)
                                    replacement-start)]
          (next-tab-trigger source-viewer (+ loffset snippet-tab-start)))
        (do (caret! source-viewer (+ loffset replacement-start (count replacement)) false)
             (show-line source-viewer)
             (remember-caret-col source-viewer (caret source-viewer)))))))

(defn show-proposals [source-viewer proposals]
  (when (pos? (count proposals))
    (let [screen-position (screen-position source-viewer)
          offset (caret source-viewer)
          result (promise)
          current-line (line source-viewer)
          target (completion-pattern nil (line-offset source-viewer) current-line offset)
          ^Stage stage (dialogs/make-proposal-popup result screen-position proposals target (text-area source-viewer))
          replace-text-fn (fn [] (when (and (realized? result) @result)
                                   (do-proposal-replacement source-viewer (first @result))
                                   (typing-changes! source-viewer)))]
      (.setOnHidden stage (ui/event-handler e (replace-text-fn))))))

(handler/defhandler :proposals :code-view
  (enabled? [source-viewer] source-viewer)
  (run [source-viewer]
    (let [proposals (propose source-viewer)]
      (if (= 1 (count proposals))
        (let [replacement (first proposals)]
          (do-proposal-replacement source-viewer replacement)
          (typing-changes! source-viewer))
        (show-proposals source-viewer proposals)))))

(defn match-open-key-typed [source-viewer key-typed]
  (let [np (caret source-viewer)
        match (get auto-matches key-typed)]
    (replace-text-and-caret source-viewer np 0 (str key-typed match) (+ np (count key-typed)))))

(defn- text-at-offset [source-viewer offset]
  (let [line-num (line-num-at-offset source-viewer offset)
        line (line-at-num source-viewer line-num)
        line-offset (line-offset source-viewer)
        rel-offset (- offset line-offset)]
    (if (< rel-offset (count line))
      (subs line rel-offset (inc rel-offset))
      "")))

(defn- part-of-string? [source-viewer]
  (let [line-text (line source-viewer)
        loffset (line-offset source-viewer)
        np (caret source-viewer)
        check-text (subs line-text 0 (- np loffset))]
    (re-find  #"^(\w|\.)+\"" (->> check-text reverse (apply str)))))

;; Handler for :key-typed should really only insert typable text. We mimic javafx and e(fx)clipse.
;;
;; javafx TextInputControlBehavior.java, defaultKeyTyped
;;
;;        // Filter out control keys except control+Alt on PC or Alt on Mac
;;        if (event.isControlDown() || event.isAltDown() || (isMac() && event.isMetaDown())) {
;;            if (!((event.isControlDown() || isMac()) && event.isAltDown())) return;
;;        }
;;
;;        // Ignore characters in the control range and the ASCII delete
;;        // character as well as meta key presses
;;        if (character.charAt(0) > 0x1F
;;            && character.charAt(0) != 0x7F
;;            && !event.isMetaDown()) { // Not sure about this one
;;
;; e(fx)clipse StyledTextBehavior.java, _keyTyped
;;
;;                      // check the modifiers
;;			// - OS-X: ALT+L ==> @
;;			// - win32/linux: ALTGR+Q ==> @
;;			if (event.isControlDown() || event.isAltDown() || (Util.isMacOS() && event.isMetaDown())) {
;;				if (!((event.isControlDown() || Util.isMacOS()) && event.isAltDown()))
;;					return;
;;			}
;;
;;			if (character.charAt(0) > 31 // No ascii control chars
;;					&& character.charAt(0) != 127 // no delete key
;;					&& !event.isMetaDown()) {

(defn- is-not-typable-modifier? [^KeyEvent e]
  (if (or (.isControlDown e)
          (.isAltDown e)
          (and (util/is-mac-os?) (.isMetaDown e)))
    (not (and (or (.isControlDown e) (util/is-mac-os?))
              (.isAltDown e)))))

(handler/defhandler :key-typed :code-view
  (enabled? [source-viewer user-data]
            (let [{:keys [key-typed ^KeyEvent key-event]} user-data]
              (and (editable? source-viewer)
                   (pos? (count key-typed))
                   (not (is-not-typable-modifier? key-event))
                   (not (.isMetaDown key-event))
                   (not (code/control-char-or-delete key-typed)))))
  (run [source-viewer user-data]
    (let [{:keys [key-typed]} user-data]
      (cond
        (and (contains? proposal-key-triggers key-typed)
             (not (part-of-string? source-viewer)))
        (do (enter-key-text source-viewer key-typed)
          (typing-changes! source-viewer true)
          (show-proposals source-viewer (propose source-viewer)))

        (and (contains? auto-match-open-set key-typed)
             (let [next-text (text-at-offset source-viewer (caret source-viewer))]
               (or (string/blank? next-text)
                   (contains? auto-match-close-set next-text))))
        (do
          (match-open-key-typed source-viewer key-typed)
          (typing-changes! source-viewer true))

        (and (contains? auto-match-close-set key-typed)
             (= (text-at-offset source-viewer (caret source-viewer)) key-typed))
        (do (caret! source-viewer (inc (caret source-viewer)) false)
          (typing-changes! source-viewer true))

        :else
        (do (enter-key-text source-viewer key-typed)
          (typing-changes! source-viewer true))))))
