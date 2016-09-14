(ns editor.code-view-ux
  (:require [clojure.string :as string]
            [editor.code :as code]
            [editor.dialogs :as dialogs]
            [editor.handler :as handler]
            [editor.ui :as ui])
  (:import  [javafx.stage Stage]
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

(def mappings
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
  :Shortcut+Left         {:command :line-begin              :label "Move to Line Begin"              :group "Movement" :order 1}
  :Ctrl+A                {:command :line-begin}
  :Shortcut+Right        {:command :line-end                :label "Move to Line End"                :group "Movement" :order 2}
  :Ctrl+E                {:command :line-end}
  :Shortcut+Up           {:command :file-begin              :label "Move to File Begin"              :group "Movement" :order 3}
  :Shortcut+Down         {:command :file-end                :label "Move to File End"                :group "Movement" :order 4}
  :Shortcut+L            {:command :goto-line               :label "Go to Line"                      :group "Movement" :order 5}

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
  :Shift+Shortcut+Left   {:command :select-line-begin}
  :Shift+Ctrl+A          {:command :select-line-begin}
  :Shift+Shortcut+Right  {:command :select-line-end}
  :Shift+Ctrl+E          {:command :select-line-end}
  :Shift+Shortcut+Up     {:command :select-file-begin}
  :Shift+Shortcut+Down   {:command :select-file-end}
  :Shift+Page-Up         {:command :select-page-up}
  :Shift+Page-Down       {:command :select-page-down}


  ;; find
  :Shortcut+F            {:command :find-text               :label "Find Text"                       :group "Find" :order 1}
  :Shortcut+K            {:command :find-next               :label "Find Next"                       :group "Find" :order 2}
  :Shortcut+G            {:command :find-next}
  :Shift+Shortcut+K      {:command :find-prev               :label "Find Prev"                       :group "Find" :order 3}
  :Shift+Shortcut+G      {:command :find-prev}

  ;; Replace
  :Shortcut+E            {:command :replace-text            :label "Replace"                         :group "Replace" :order 1}
  :Alt+Shortcut+E        {:command :replace-next            :label "Replace Next"                    :group "Replace" :order 2}

  ;; Delete
  :Backspace             {:command :delete}
  :Delete                {:command :delete-forward}
  :Shortcut+D            {:command :cut                     :label "Delete Line"                     :group "Delete" :order 1}
  :Alt+Delete            {:command :delete-next-word}  ;; these two do not work when they are included in the menu
  :Alt+Backspace         {:command :delete-prev-word}  ;; the menu event does not get propagated back like the rest
  :Shortcut+Delete       {:command :delete-to-start-of-line :label "Delete to the Start of the Line" :group "Delete" :order 4}
  :Shift+Shortcut+Delete {:command :delete-to-end-of-line   :label "Delete to the End of the Line"   :group "Delete" :order 5}
  :Ctrl+K                {:command :cut-to-end-of-line}

  ;; Comment
  :Shortcut+Slash        {:command :toggle-comment          :label "Toggle Comment"                  :group "Comment" :order 1}

  ;; Editing
  :Tab                   {:command :tab}
  :Shift+Tab             {:command :backwards-tab-trigger}
  :Enter                 {:command :enter}

  ;; Paste
  :Ctrl+Y                {:command :paste}

  ;;Completions
  :Ctrl+Space            {:command :proposals}

  ;;Indentation
  :Shortcut+I            {:command :indent                  :label "Indent"                           :group "Indent" :order 1}

})

(def proposal-key-triggers #{"."})

(defn menu-data [item]
  {:label (:label (val item))
   :acc (name (key item))
   :command (:command (val item))
   :order (:order (val item))})

(defn create-menu-data []
  (let [movement-commands (filter (fn [[k v]] (= "Movement" (:group v))) mappings)
        find-commands (filter (fn [[k v]] (= "Find" (:group v))) mappings)
        replace-commands (filter (fn [[k v]] (= "Replace" (:group v))) mappings)
        delete-commands (filter (fn [[k v]] (= "Delete" (:group v))) mappings)
        comment-commands (filter (fn [[k v]] (= "Comment" (:group v))) mappings)
        indent-commands (filter (fn [[k v]] (= "Indent" (:group v))) mappings)]
    [{:label "Text Edit"
       :id ::text-edit
       :children (concat
                  (sort-by :order (map menu-data movement-commands))
                  [{:label :separator}]
                  (sort-by :order (map menu-data find-commands))
                  [{:label :separator}]
                  (sort-by :order (map menu-data replace-commands))
                  [{:label :separator}]
                  (sort-by :order (map menu-data delete-commands))
                  [{:label :separator}]
                  (sort-by :order (map menu-data comment-commands))
                  [{:label :separator}]
                  (sort-by :order (map menu-data indent-commands)))}]))

;; keep regex's in split-indentation etc in sync with these
(def tab-size 4)
(def default-indentation "\t")

;; these are global for now, reasonable to search for same thing in several files
(def last-find-text (atom ""))
(def last-replace-text (atom ""))

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
   :meta? (.isShortcutDown ^KeyEvent e)
   :alt? (.isAltDown ^KeyEvent e)
   :shift? (.isShiftDown ^KeyEvent e)})

(defn- add-modifier [code-str info modifier-key modifier-str]
  (if (get info modifier-key) (str modifier-str code-str) code-str))

;; potentially slow with each new keyword that is created
(defn- key-fn [info code]
  (let [code (-> (.getName ^KeyCode code)
                 (string/replace #" " "-")
                 (add-modifier info :meta? "Shortcut+")
                 (add-modifier info :alt? "Alt+")
                 (add-modifier info :control? "Ctrl+")
                 (add-modifier info :shift? "Shift+")
                 (keyword))]
    (get mappings code)))

(defn- click-fn [click-count]
  (let [code (case (int click-count)
               3 :Triple-Click
               2 :Double-Click
               :Single-Click)]
    (get mappings code)))

(defn- is-mac-os? []
  (= "Mac OS X" (System/getProperty "os.name")))

(defn handler-run [command command-contexts user-data]
  (-> (handler/active command command-contexts user-data)
      handler/run))

(defn handle-key-pressed [^KeyEvent e source-viewer]
  (let [k-info (info e)
        kf (key-fn k-info (.getCode e))]
    (when (not (.isConsumed e))
      (.consume e)
      (when (and (:command kf) (not (:label kf)))
        (handler-run
          (:command kf)
          [{:name :code-view :env {:selection source-viewer :clipboard (Clipboard/getSystemClipboard)}}]
          k-info)
        (last-command! source-viewer (:command kf))))))

(defn is-not-typable-modifier? [e]
  (if (or (.isControlDown ^KeyEvent e)
          (.isAltDown ^KeyEvent e)
          (and (is-mac-os?)
               (.isMetaDown ^KeyEvent e)))
    (not (or (.isControlDown ^KeyEvent e)
             (and (is-mac-os?)
                  (.isAltDown ^KeyEvent e))))))

(defn handle-key-typed [^KeyEvent e source-viewer]
  (let [key-typed (.getCharacter e)]
    (when-not (or (.isMetaDown ^KeyEvent e)
                  (is-not-typable-modifier? e))
      ;; duplicated precondition from :key-typed defhandler in order to
      ;; update last-command properly
      (when (and (editable? selection)
               (pos? (count key-typed))
               (code/not-control-char-or-delete key-typed))
        (handler-run
         :key-typed
         [{:name :code-view :env {:selection source-viewer :key-typed key-typed}}]
         e)
        (last-command! source-viewer :key-typed)))))

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
        text-before (subs line-text 0 (- np line-offset))]
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
        [{:name :code-view :env {:selection source-viewer :clipboard (Clipboard/getSystemClipboard)}}]
        e)
      (last-command! source-viewer (:command cf)))
    (.consume e)))

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

;; "selection" in handlers is actually the source-viewer

(handler/defhandler :copy :code-view
  (enabled? [selection] selection)
  (run [selection clipboard]
    (text! clipboard (text-selection selection))))

(handler/defhandler :paste :code-view
  (enabled? [selection] (editable? selection))
  (run [selection clipboard]
    (when-let [clipboard-text (text clipboard)]
      (let [code (text selection)
            caret (caret selection)]
        (if (pos? (selection-length selection))
          (replace-text-selection selection clipboard-text)
          (replace-text-and-caret selection caret 0 clipboard-text (+ caret (count clipboard-text))))
        (typing-changes! selection)))))

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
  (enabled? [selection] selection)
  (run [selection]
    (right selection)
    (state-changes! selection)))

(handler/defhandler :left :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (left selection)
    (state-changes! selection)))

(handler/defhandler :select-right :code-view
  (enabled? [selection] selection)
  (run [selection]
    (select-right selection)
    (state-changes! selection)))

(handler/defhandler :select-left :code-view
  (enabled? [selection] selection)
  (run [selection]
    (select-left selection)
    (state-changes! selection)))

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
  (enabled? [selection] selection)
  (run [selection]
    (up selection true)
    (state-changes! selection)))

(handler/defhandler :down :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (down selection true)
    (state-changes! selection)))

(handler/defhandler :select-up :code-view
  (enabled? [selection] selection)
  (run [selection]
    (select-up selection true)
    (state-changes! selection)))

(handler/defhandler :select-down :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (select-down selection true)
    (state-changes! selection)))

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
  (enabled? [selection] selection)
  (run [selection]
    (do-page-up selection false)
    (state-changes! selection)))

(handler/defhandler :page-down :code-view
  (enabled? [selection] selection)
  (run [selection]
    (do-page-down selection false)
    (state-changes! selection)))

(handler/defhandler :select-page-up :code-view
  (enabled? [selection] selection)
  (run [selection]
    (do-page-up selection true)
    (state-changes! selection)))

(handler/defhandler :select-page-down :code-view
  (enabled? [selection] selection)
  (run [selection]
    (do-page-down selection true)
    (state-changes! selection)))

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
  (enabled? [selection] selection)
  (run [selection]
    (next-word selection false)
    (state-changes! selection)))

(handler/defhandler :prev-word :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (prev-word selection false)
    (state-changes! selection)))

(handler/defhandler :select-next-word :code-view
  (enabled? [selection] selection)
  (run [selection]
    (next-word selection true)
    (state-changes! selection)))

(handler/defhandler :select-prev-word :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (prev-word selection true)
    (state-changes! selection)))

(defn line-begin [source-viewer select?]
  (let [next-pos (line-offset source-viewer)]
    (caret! source-viewer next-pos select?)
    (remember-caret-col source-viewer next-pos)))

(defn line-end-pos [source-viewer]
  (let [doc (text source-viewer)
        line-text-len (count (line source-viewer))
        line-offset (line-offset source-viewer)]
    (+ line-offset line-text-len)))

(defn line-end [source-viewer select?]
  (let [next-pos (line-end-pos source-viewer)]
    (caret! source-viewer next-pos select?)
    (remember-caret-col source-viewer next-pos)))

(handler/defhandler :line-begin :code-view
  (enabled? [selection] selection)
  (run [selection]
    (line-begin selection false)
    (state-changes! selection)))

(handler/defhandler :line-end :code-view
  (enabled? [selection] selection)
  (run [selection]
    (line-end selection false)
    (state-changes! selection)))

(handler/defhandler :select-line-begin :code-view
  (enabled? [selection] selection)
  (run [selection]
    (line-begin selection true)
    (state-changes! selection)))

(handler/defhandler :select-line-end :code-view
  (enabled? [selection] selection)
  (run [selection]
    (line-end selection true)
    (state-changes! selection)))

(handler/defhandler :file-begin :code-view
  (enabled? [selection] selection)
  (run [selection]
    (caret! selection 0 false)
    (show-line selection)
    (remember-caret-col selection 0)
    (state-changes! selection)))

(handler/defhandler :file-end :code-view
  (enabled? [selection] selection)
  (run [selection]
    (let [doc (text selection)]
      (caret! selection (count doc) false)
      (show-line selection)
      (remember-caret-col selection (caret selection))
      (state-changes! selection))))

(handler/defhandler :select-file-begin :code-view
  (enabled? [selection] selection)
  (run [selection]
    (caret! selection 0 true)
    (show-line selection)
    (remember-caret-col selection 0)
    (state-changes! selection)))

(handler/defhandler :select-file-end :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [doc (text selection)]
      (caret! selection (count doc) true)
      (show-line selection)
      (remember-caret-col selection (caret selection))
      (state-changes! selection))))

(defn go-to-line [source-viewer line-number]
  (when line-number
    (try
     (let [line (Integer/parseInt line-number)
           line-count (line-count source-viewer)
           line-num (if (> 1 line) 0 (dec line))
           np (if (>= line-count line-num)
                (line-offset-at-num source-viewer line-num)
                (count (text source-viewer)))]
       (caret! source-viewer np false)
       (remember-caret-col source-viewer np)
       (show-line source-viewer))
     (catch Throwable  e (println "Not a valid line number" line-number (.getMessage e))))))

(handler/defhandler :goto-line :code-view
  (enabled? [selection] selection)
  (run [selection]
    ;; when using show and wait on the dialog, was getting very
    ;; strange double events not solved by consume - this is a
    ;; workaround to let us use show
    (let [line-number (promise)
          ^Stage stage (dialogs/make-goto-line-dialog line-number)
          go-to-line-fn (fn [] (when (realized? line-number)
                                 (go-to-line selection @line-number)
                                 (state-changes! selection)))]
      (.setOnHidden stage (ui/event-handler e (go-to-line-fn))))))

(handler/defhandler :select-word :code-view
  (enabled? [selection] selection)
  (run [selection]
    (let [regex #"^\w+"
          np (caret selection)
          doc (text selection)
          word-end (re-find regex (subs doc np))
          text-before (->> (subs doc 0 (adjust-bounds doc (inc np))) (reverse) (rest) (apply str))
          word-begin (re-find regex text-before)
          start-pos (- np (count word-begin))
          end-pos (+ np (count word-end))
          len (- end-pos start-pos)]
      (text-selection! selection start-pos len)
      (state-changes! selection))))

(handler/defhandler :select-line :code-view
  (enabled? [selection] selection)
  (run [selection]
    (let [regex #"^\w+"
          line (line selection)
          line-offset (line-offset selection)]
      (text-selection! selection line-offset (count line))
      (state-changes! selection))))

(handler/defhandler :select-all :code-view
  (enabled? [selection] selection)
  (run [selection]
    (caret! selection (count (text selection)) false)
    (text-selection! selection 0 (count (text selection)))
    (remember-caret-col selection (caret selection))
    (state-changes! selection)))

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

(handler/defhandler :delete :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (when (editable? selection)
      (delete selection false)
      (typing-changes! selection true))))

(handler/defhandler :delete-forward :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (when (editable? selection)
      (delete selection true)
      (typing-changes! selection true))))

(defn cut-selection [source-viewer clipboard]
  (text! clipboard (text-selection source-viewer))
  (replace-text-selection source-viewer ""))

(defn cut-line [source-viewer clipboard]
  (let [np (caret source-viewer)
        doc (text source-viewer)
        line-begin-offset (line-offset source-viewer)
        line-end-offset (line-end-pos source-viewer)
        consume-pos (if (= line-end-offset (count doc))
                      line-end-offset
                      (inc line-end-offset))
        new-doc (str (subs doc 0 line-begin-offset)
                     (subs doc consume-pos))
        line-doc (str (subs doc line-begin-offset consume-pos))]
    (text! clipboard line-doc)
    (text! source-viewer new-doc)
    (caret! source-viewer (adjust-bounds new-doc line-begin-offset) false)
    (remember-caret-col source-viewer (caret source-viewer))
    (show-line source-viewer)))

(handler/defhandler :cut :code-view
  (enabled? [selection] (editable? selection))
  (run [selection clipboard]
    (if (pos? (selection-length selection))
      (cut-selection selection clipboard)
      (cut-line selection clipboard))
    (typing-changes! selection)))

(handler/defhandler :delete-prev-word :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (let [np (caret selection)
          doc (text selection)
          next-word-move (prev-word-move doc np)
          new-pos (- np (count next-word-move))
          new-doc (str (subs doc 0 new-pos)
                       (subs doc np))]
      (text! selection new-doc)
      (caret! selection new-pos false)
      (remember-caret-col selection (caret selection))
      (show-line selection))
    (typing-changes! selection)))

(handler/defhandler :delete-next-word :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (let [np (caret selection)
          doc (text selection)
          next-word-move (next-word-move doc np)
          next-word-pos (+ np (count next-word-move))
          new-doc (str (subs doc 0 np)
                       (subs doc next-word-pos))]
      (text! selection new-doc))
    (typing-changes! selection)))

(handler/defhandler :delete-to-end-of-line :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (let [np (caret selection)
          doc (text selection)
          line-end-offset (line-end-pos selection)
          new-doc (str (subs doc 0 np)
                       (subs doc line-end-offset))]
      (text! selection new-doc))
    (typing-changes! selection)))

(handler/defhandler :cut-to-end-of-line :code-view
  (enabled? [selection] (editable? selection))
  (run [selection clipboard]
    (let [np (caret selection)
          doc (text selection)
          line-end-offset (line-end-pos selection)
          consume-pos (if (= line-end-offset np) (adjust-bounds doc (inc line-end-offset)) line-end-offset)
          new-doc (str (subs doc 0 np)
                       (subs doc consume-pos))
          clipboard-text (if (= :cut-to-end-of-line (last-command selection))
                           (str (text clipboard) (subs doc np consume-pos))
                           (subs doc np consume-pos))]
      (text! clipboard clipboard-text)
      (text! selection new-doc))
    (typing-changes! selection)))

(handler/defhandler :delete-to-start-of-line :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (let [np (caret selection)
          doc (text selection)
          line-begin-offset (line-offset selection)
          new-doc (str (subs doc 0 line-begin-offset)
                       (subs doc np))]
      (text! selection new-doc)
      (caret! selection line-begin-offset false)
      (remember-caret-col selection line-begin-offset))
    (typing-changes! selection)))

(defn select-found-text [source-viewer doc found-idx tlen]
  (when (<= 0 found-idx)
    (caret! source-viewer (adjust-bounds doc (+ found-idx tlen)) false)
    (show-line source-viewer)
    (text-selection! source-viewer found-idx tlen)
    (remember-caret-col source-viewer (caret source-viewer))))

(defn find-text [source-viewer find-text]
  (let [doc (text source-viewer)
        found-idx (.indexOf ^String doc ^String find-text)
        tlen (count find-text)]
    (select-found-text source-viewer doc found-idx tlen)))

(handler/defhandler :find-text :code-view
  (enabled? [selection] selection)
  (run [selection]
    ;; when using show and wait on the dialog, was getting very
    ;; strange double events not solved by consume - this is a
    ;; workaround to let us use show
    (let [text (promise)
          ^Stage stage (dialogs/make-find-text-dialog text)
          find-text-fn (fn [] (when (realized? text)
                                (find-text selection (or @text ""))
                                (state-changes! selection)))]
      (.setOnHidden stage (ui/event-handler e (find-text-fn))))))

(handler/defhandler :find-next :code-view
  (enabled? [selection] selection)
  (run [selection]
    (let [np (caret selection)
          doc (text selection)
          search-text (text-selection selection)
          found-idx (.indexOf ^String doc ^String search-text ^int np)
          tlen (count search-text)]
      (select-found-text selection doc found-idx tlen)
      (state-changes! selection))))

(handler/defhandler :find-prev :code-view
  (enabled? [selection] selection)
  (run [selection]
    (let [np (caret selection)
          doc (text selection)
          search-text (text-selection selection)
          tlen (count search-text)
          found-idx (.lastIndexOf ^String doc ^String search-text ^int (adjust-bounds doc (- np (inc tlen))))]
      (select-found-text selection doc found-idx tlen)
      (state-changes! selection))))

(defn do-replace [source-viewer doc found-idx rtext tlen tlen-new caret-pos]
  (when (<= 0 found-idx)
    (replace! source-viewer found-idx tlen rtext)
    (caret! source-viewer (adjust-bounds (text source-viewer) (+ tlen-new found-idx)) false)
    (show-line source-viewer)
    (text-selection! source-viewer found-idx tlen-new)
    (remember-caret-col source-viewer (caret source-viewer))
    ;;Note:  trying to highlight the selection doensn't
    ;; work quite right due to rendering problems in the StyledTextSkin
    ))

(defn replace-text [source-viewer {ftext :find-text rtext :replace-text :as result}]
  (when (and ftext rtext)
    (let [doc (text source-viewer)
          found-idx (.indexOf ^String doc ^String ftext)
          tlen (count ftext)
          tlen-new (count rtext)]
      (do-replace source-viewer doc found-idx rtext tlen tlen-new 0)
      (reset! last-find-text ftext)
      (reset! last-replace-text rtext))))

(handler/defhandler :replace-text :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    ;; when using show and wait on the dialog, was getting very
    ;; strange double events not solved by consume - this is a
    ;; workaround to let us use show
    (let [result (promise)
          ^Stage stage (dialogs/make-replace-text-dialog result)
          replace-text-fn (fn []
                            (when (realized? result)
                              (replace-text selection (or @result {}))
                              (typing-changes! selection)))]
      (.setOnHidden stage (ui/event-handler e (replace-text-fn))))))

(handler/defhandler :replace-next :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (let [np (caret selection)
          doc (text selection)
          ftext @last-find-text
          found-idx (string/index-of doc ftext np)
          tlen (count ftext)
          rtext @last-replace-text
          tlen-new (count rtext)]
      (when found-idx
        (do-replace selection doc found-idx rtext tlen tlen-new np)
        (typing-changes! selection)))))

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

(defn comment-region [source-viewer]
  (let [line-comment (:line-comment (syntax source-viewer))
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
  (let [line-comment (:line-comment (syntax source-viewer))
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
  (enabled? [selection] (editable? selection))
  (run [selection]
    (if (pos? (count (text-selection selection)))
      (if (every? #(commented? (syntax selection) %) (selected-lines selection))
        (uncomment-region selection)
        (comment-region selection))
      (toggle-line-comment selection))
    (typing-changes! selection)))

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
            (select-found-text source-viewer doc found-idx tlen)))))))

(defn prev-tab-trigger [source-viewer pos]
  (let [doc (text source-viewer)
        tab-trigger-info (prev-snippet-tab-trigger! source-viewer)
        search-text (:trigger tab-trigger-info)]
    (when-not (= :begin search-text)
      (let [found-idx (string/last-index-of doc search-text pos)
            tlen (count search-text)]
        (when found-idx
          (select-found-text source-viewer doc found-idx tlen))))))

(handler/defhandler :tab :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (when (editable? selection)
      (if (has-snippet-tab-trigger? selection)
        (let [np (caret selection)
              doc (text selection)]
          (next-tab-trigger selection np)
          (typing-changes! selection))
        (do (enter-key-text selection "\t")
            (typing-changes! selection))))))

(handler/defhandler :backwards-tab-trigger :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (when (editable? selection)
      (if (has-prev-snippet-tab-trigger? selection)
        (let [np (caret selection)
              doc (text selection)]
          (prev-tab-trigger selection np)
          (state-changes! selection))))))

(defn- trim-indentation [line]
  (second (re-find #"^[\t ]*(.*)" line)))

(defn- get-indentation [line]
  (re-find #"^[\t ]*" line))

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

(handler/defhandler :indent :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (when (editable? selection)
      (if (pos? (count (text-selection selection)))
        (let [region-start (selection-offset selection)
              region-len (selection-length selection)
              region-end (+ region-start region-len)]
          (do-indent-region selection region-start region-end))
        (let [line-num (line-num-at-offset selection (caret selection))]
          (do-indent-line selection line-num)))
      (typing-changes! selection))))

(handler/defhandler :enter :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (when (editable? selection)
      (clear-snippet-tab-triggers! selection)
      (if (= (caret selection) (line-end-pos selection))
        (do
          (do-unindent-line selection (line-num-at-offset selection (caret selection)))
          (enter-key-text selection (System/getProperty "line.separator"))
          (do-indent-line selection (line-num-at-offset selection (caret selection)))
          (caret! selection (+ (line-offset selection) (count (line selection))) false)
          (remember-caret-col selection (caret selection))
          (show-line selection))
        (enter-key-text selection (System/getProperty "line.separator")))
      (typing-changes! selection))))

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
          ^Stage stage (dialogs/make-proposal-dialog result screen-position proposals target (text-area source-viewer))
          replace-text-fn (fn [] (when (and (realized? result) @result)
                                   (do-proposal-replacement source-viewer (first @result))
                                   (typing-changes! source-viewer)))]
      (.setOnHidden stage (ui/event-handler e (replace-text-fn))))))

(handler/defhandler :proposals :code-view
  (enabled? [selection] selection)
  (run [selection]
    (let [proposals (propose selection)]
      (if (= 1 (count proposals))
        (let [replacement (first proposals)]
          (do-proposal-replacement selection replacement)
          (typing-changes! selection))
        (show-proposals selection proposals)))))

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

(handler/defhandler :key-typed :code-view
  (enabled? [selection] (editable? selection))
  (run [selection key-typed]
    (when (and (editable? selection)
               (pos? (count key-typed))
               (code/not-control-char-or-delete key-typed))
      (cond
        (and (contains? proposal-key-triggers key-typed)
             (not (part-of-string? selection)))
        (do (enter-key-text selection key-typed)
            (typing-changes! selection true)
            (show-proposals selection (propose selection)))

        (and (contains? auto-match-open-set key-typed)
             (let [next-text (text-at-offset selection (caret selection))]
               (or (string/blank? next-text)
                   (contains? auto-match-close-set next-text))))
        (do 
          (match-open-key-typed selection key-typed)
          (typing-changes! selection true))

        (and (contains? auto-match-close-set key-typed)
             (= (text-at-offset selection (caret selection)) key-typed))
        (do (caret! selection (inc (caret selection)) false)
            (typing-changes! selection true))

        :else
        (do (enter-key-text selection key-typed)
            (typing-changes! selection true))))))
