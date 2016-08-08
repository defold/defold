(ns editor.code-view-ux
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
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
  (next-line [this])
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
  (typing-changes! [this])
  (changes! [this]))

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

(def tab-size 4)
(def last-find-text (atom ""))
(def last-replace-text (atom ""))
(def last-command (atom nil))
(def auto-matches {"\"" "\""
                   "[" "]"
                   "{" "}"
                   "(" ")"})
(def auto-match-set (into #{} (keys auto-matches)))
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
        (reset! last-command (:command kf))))))

(defn is-not-typable-modifier? [e]
  (if (or (.isControlDown ^KeyEvent e) (.isAltDown ^KeyEvent e) (and (is-mac-os?) (.isMetaDown ^KeyEvent e)))
    (not (or (.isControlDown ^KeyEvent e) (and (is-mac-os?) (.isAltDown ^KeyEvent e))))))

(defn handle-key-typed [^KeyEvent e source-viewer]
  (let [key-typed (.getCharacter e)]
    (when-not (or (.isMetaDown ^KeyEvent e) (is-not-typable-modifier? e))
      (handler-run
        :key-typed
        [{:name :code-view :env {:selection source-viewer :key-typed key-typed}}]
        e))))

(defn adjust-bounds [s pos]
  (if (neg? pos) 0 (min (count s) pos)))

(defn tab-count [s]
  (let [tab-count (count (filter #(= \tab %) s))]
    tab-count))

(defn remember-caret-col [selection np]
  (let [line-offset (line-offset selection)
        line-text (line selection)
        text-before (subs line-text 0 (- np line-offset))
        tab-count (tab-count text-before)
        caret-col (+ (count text-before) (* tab-count (dec tab-size)))]
    (preferred-offset! selection caret-col)))

(defn handle-mouse-clicked [^MouseEvent e source-viewer]
  (let [click-count (.getClickCount e)
        cf (click-fn click-count)
        pos (caret source-viewer)]
    (remember-caret-col source-viewer pos)
    (clear-snippet-tab-triggers! source-viewer)
    (reset! last-command nil)
    (when cf
      (handler-run
        (:command cf)
        [{:name :code-view :env {:selection source-viewer :clipboard (Clipboard/getSystemClipboard)}}]
        e))
    (.consume e)))

(defn- adjust-replace-length [doc pos n]
  (if (> (+ pos n) (count doc))
    0
    n))

(defn- replace-text-selection [selection s]
  (let [np (selection-offset selection)
        sel-len (selection-length selection)]
    (text-selection! selection 0 0)
    (replace! selection np sel-len s)
    (caret! selection (inc np) false)
    (show-line selection)))

(defn- replace-text-and-caret [selection offset length s new-caret-pos]
  (let [doc (text selection)
        pos (adjust-bounds doc offset)
        new-len (adjust-replace-length (text selection) pos length)
        new-pos (adjust-bounds doc new-caret-pos)]
    (replace! selection pos new-len s)
    (caret! selection (adjust-bounds (text selection) new-caret-pos) false)
    (show-line selection)
    (remember-caret-col selection new-caret-pos)))

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
          (replace-text-and-caret selection caret 0 clipboard-text (+ caret (count clipboard-text))))))))

(defn right [selection]
  (let [c (caret selection)
        doc (text selection)
        selected-text (text-selection selection)
        next-pos (if (pos? (count selected-text))
                   (adjust-bounds doc (+ c (count selected-text)))
                   (adjust-bounds doc (inc c)))]
    (clear-snippet-tab-triggers! selection)
    (caret! selection next-pos false)
    (remember-caret-col selection next-pos)))

(defn select-right [selection]
  (let [c (caret selection)
        doc (text selection)
        selected-text (text-selection selection)
        next-pos (adjust-bounds doc (inc c))]
    (clear-snippet-tab-triggers! selection)
    (caret! selection next-pos true)
    (remember-caret-col selection next-pos)))

(defn left [selection]
  (let [c (caret selection)
        doc (text selection)
        selected-text (text-selection selection)
        next-pos (if (pos? (count selected-text))
                   (adjust-bounds doc (- c (count selected-text)))
                   (adjust-bounds doc (dec c)))]
    (clear-snippet-tab-triggers! selection)
    (caret! selection next-pos false)
    (remember-caret-col selection next-pos)))

(defn select-left [selection]
  (let [c (caret selection)
        doc (text selection)
        selected-text (text-selection selection)
        next-pos (adjust-bounds doc (dec c))]
    (clear-snippet-tab-triggers! selection)
    (caret! selection next-pos true)
    (remember-caret-col selection next-pos)))

(handler/defhandler :right :code-view
  (enabled? [selection] selection)
  (run [selection]
    (right selection)))

(handler/defhandler :left :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (left selection)))

(handler/defhandler :select-right :code-view
  (enabled? [selection] selection)
  (run [selection]
    (select-right selection)))

(handler/defhandler :select-left :code-view
  (enabled? [selection] selection)
  (run [selection]
    (select-left selection)))

(defn up-line [selection preferred-offset]
  (let [pos (caret selection)
        line-offset (line-offset selection)
        line-before (prev-line selection)
        len-before (- pos line-offset)
        prev-line-tabs (tab-count line-before)
        prev-line-len (count line-before)
        preferred-next-pos (min preferred-offset (+ prev-line-len (* prev-line-tabs (dec tab-size))))
        actual-next-pos (- preferred-next-pos (* prev-line-tabs (dec tab-size)))
        adj-next-pos (+ (- pos len-before 1 prev-line-len)
                        (if (neg? actual-next-pos) 0 actual-next-pos))]
      (adjust-bounds (text selection) adj-next-pos)))

(defn down-line [selection preferred-offset]
  (let [pos (caret selection)
        line-offset (line-offset selection)
        line-text-len (count (line selection))
        len-after (- (+ line-text-len line-offset) pos)
        line-after (next-line selection)
        next-line-tabs (tab-count line-after)
        next-line-len (count line-after)
        preferred-next-pos (min preferred-offset (+ next-line-len (* next-line-tabs (dec tab-size))))
        actual-next-pos (- preferred-next-pos (* next-line-tabs (dec tab-size)))
        adj-next-pos (+ pos len-after 1 (if (neg? actual-next-pos) 0 actual-next-pos))]
      (adjust-bounds (text selection) adj-next-pos)))

(defn up [selection show?]
  (let [doc (text selection)
        preferred-offset (preferred-offset selection)
        next-pos (if (pos? (selection-length selection))
                   (adjust-bounds doc (selection-offset selection))
                   (up-line selection preferred-offset))]
    (caret! selection next-pos false)
    (when show? (show-line selection))))

(defn select-up [selection show?]
  (let [doc (text selection)
        preferred-offset (preferred-offset selection)
        next-pos (up-line selection preferred-offset)
        start-of-doc? (= next-pos 0)]
    (if (and (not start-of-doc?)
             (= (selection-offset selection) next-pos))
      (caret! selection next-pos false)
      (caret! selection next-pos true))
    (when show? (show-line selection))))

(defn down [selection show?]
  (let [doc (text selection)
        preferred-offset (preferred-offset selection)
        next-pos (if (pos? (selection-length selection))
                   (adjust-bounds doc (+ (selection-offset selection)
                                         (selection-length selection)))
                   (down-line selection preferred-offset))]
      (caret! selection next-pos false)
      (when show? (show-line selection))))

(defn select-down [selection show?]
  (let [doc (text selection)
        preferred-offset (preferred-offset selection)
        next-pos (down-line selection preferred-offset)
        end-of-doc? (= next-pos (count doc))]
    (if (and (not end-of-doc?)
             (= (+ (selection-length selection) (selection-offset selection)) next-pos))
      (caret! selection next-pos false)
      (caret! selection next-pos true)))
  (when show? (show-line selection)))

(handler/defhandler :up :code-view
  (enabled? [selection] selection)
  (run [selection]
    (up selection true)))

(handler/defhandler :down :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (down selection true)))

(handler/defhandler :select-up :code-view
  (enabled? [selection] selection)
  (run [selection]
    (select-up selection true)))

(handler/defhandler :select-down :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (select-down selection true)))

(defn do-page-up [selection with-select?]
 (let [first-line (first-visible-row-number selection)
       last-line (last-visible-row-number selection)]
   (page-up selection)
   (dotimes [n (- last-line first-line)]
     (if with-select?
       (select-up selection false)
       (up selection false)))))

(defn do-page-down [selection with-select?]
    (let [first-line (first-visible-row-number selection)
          last-line (last-visible-row-number selection)]
      (page-down selection)
      (dotimes [n (- last-line first-line)]
       (if with-select?
         (select-down selection false)
         (down selection false)))))

(handler/defhandler :page-up :code-view
  (enabled? [selection] selection)
  (run [selection]
    (do-page-up selection false)))

(handler/defhandler :page-down :code-view
  (enabled? [selection] selection)
  (run [selection]
    (do-page-down selection false)))

(handler/defhandler :select-page-up :code-view
  (enabled? [selection] selection)
  (run [selection]
    (do-page-up selection true)))

(handler/defhandler :select-page-down :code-view
  (enabled? [selection] selection)
  (run [selection]
    (do-page-down selection true)))

(def word-regex  #"\n|\s*_*[a-zA-Z0-9\=\+\-\*\!]+|.")

(defn next-word-move [doc np]
  (re-find word-regex (subs doc np)))

(defn next-word [selection select?]
  (let [np (caret selection)
        doc (text selection)
        next-word-move (next-word-move doc np)
        next-pos (+ np (count next-word-move))]
    (caret! selection next-pos select?)
    (show-line selection)
    (remember-caret-col selection next-pos)))

(defn prev-word-move [doc np]
  (re-find word-regex (->> (subs doc 0 np) (reverse) (apply str))))

(defn prev-word [selection select?]
  (let [np (caret selection)
        doc (text selection)
        next-word-move (prev-word-move doc np)
        next-pos (- np (count next-word-move))]
    (caret! selection next-pos select?)
    (show-line selection)
    (remember-caret-col selection next-pos)))

(handler/defhandler :next-word :code-view
  (enabled? [selection] selection)
  (run [selection]
    (next-word selection false)))

(handler/defhandler :prev-word :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (prev-word selection false)))

(handler/defhandler :select-next-word :code-view
  (enabled? [selection] selection)
  (run [selection]
    (next-word selection true)))

(handler/defhandler :select-prev-word :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (prev-word selection true)))

(defn line-begin [selection select?]
  (let [next-pos (line-offset selection)]
    (caret! selection next-pos select?)
    (remember-caret-col selection next-pos)))

(defn line-end-pos [selection]
  (let [doc (text selection)
        line-text-len (count (line selection))
        line-offset (line-offset selection)]
    (+ line-offset line-text-len)))

(defn line-end [selection select?]
  (let [next-pos (line-end-pos selection)]
    (caret! selection next-pos select?)
    (remember-caret-col selection next-pos)))

(handler/defhandler :line-begin :code-view
  (enabled? [selection] selection)
  (run [selection]
    (line-begin selection false)))

(handler/defhandler :line-end :code-view
  (enabled? [selection] selection)
  (run [selection]
    (line-end selection false)))

(handler/defhandler :select-line-begin :code-view
  (enabled? [selection] selection)
  (run [selection]
    (line-begin selection true)))

(handler/defhandler :select-line-end :code-view
  (enabled? [selection] selection)
  (run [selection]
    (line-end selection true)))

(handler/defhandler :file-begin :code-view
  (enabled? [selection] selection)
  (run [selection]
    (caret! selection 0 false)
    (show-line selection)))

(handler/defhandler :file-end :code-view
  (enabled? [selection] selection)
  (run [selection]
    (let [doc (text selection)]
      (caret! selection (count doc) false)
      (show-line selection))))

(handler/defhandler :select-file-begin :code-view
  (enabled? [selection] selection)
  (run [selection]
    (caret! selection 0 true)
    (show-line selection)))

(handler/defhandler :select-file-end :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [doc (text selection)]
      (caret! selection (count doc) true)
      (show-line selection))))

(defn go-to-line [selection line-number]
  (when line-number
    (try
     (let [line (Integer/parseInt line-number)
           line-count (line-count selection)
           line-num (if (> 1 line) 0 (dec line))
           np (if (>= line-count line-num)
                (line-offset-at-num selection line-num)
                (count (text selection)))]
       (caret! selection np false)
       (show-line selection))
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
                                (go-to-line selection @line-number)))]
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
      (text-selection! selection start-pos len))))

(handler/defhandler :select-line :code-view
  (enabled? [selection] selection)
  (run [selection]
    (let [regex #"^\w+"
          line (line selection)
          line-offset (line-offset selection)]
      (text-selection! selection line-offset (count line)))))

(handler/defhandler :select-all :code-view
  (enabled? [selection] selection)
  (run [selection]
    (caret! selection (count (text selection)) false)
    (text-selection! selection 0 (count (text selection)))))

(defn match-key-typed [selection key-typed]
  (let [np (caret selection)
        match (get auto-matches key-typed)]
       (replace-text-and-caret selection np 0 match np)))

(defn delete [selection forward?]
  (let [np (caret selection)
        doc (text selection)
        slen (selection-length selection)
        soffset (selection-offset selection)]
    (if (pos? slen)
      (replace-text-and-caret selection soffset slen "" soffset)
      (let [pos (if forward? np (adjust-bounds doc (dec np)))
            target (subs doc pos (adjust-bounds doc (+ 2 pos)))]
       (when-not (and (zero? np) (not forward?))
         (if (contains? auto-delete-set target)
           (replace-text-and-caret selection pos 2 "" pos)
           (replace-text-and-caret selection pos 1 "" pos)))))))

(handler/defhandler :delete :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (when (editable? selection)
      (changes! selection)
      (delete selection false))))

(handler/defhandler :delete-forward :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (when (editable? selection)
      (changes! selection)
      (delete selection true))))

(defn cut-selection [selection clipboard]
  (text! clipboard (text-selection selection))
  (replace-text-selection selection ""))

(defn cut-line [selection clipboard]
  (let [np (caret selection)
        doc (text selection)
        line-begin-offset (line-offset selection)
        line-end-offset (line-end-pos selection)
        consume-pos (if (= line-end-offset (count doc))
                      line-end-offset
                      (inc line-end-offset))
        new-doc (str (subs doc 0 line-begin-offset)
                     (subs doc consume-pos))
        line-doc (str (subs doc line-begin-offset consume-pos))]
    (text! clipboard line-doc)
    (text! selection new-doc)
    (caret! selection (adjust-bounds new-doc line-begin-offset) false)
    (show-line selection)))

(handler/defhandler :cut :code-view
  (enabled? [selection] (editable? selection))
  (run [selection clipboard]
    (changes! selection)
    (if (pos? (selection-length selection))
      (cut-selection selection clipboard)
      (cut-line selection clipboard))))

(handler/defhandler :delete-prev-word :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (changes! selection)
    (let [np (caret selection)
          doc (text selection)
          next-word-move (prev-word-move doc np)
          new-pos (- np (count next-word-move))
          new-doc (str (subs doc 0 new-pos)
                       (subs doc np))]
      (text! selection new-doc)
      (caret! selection new-pos false)
      (show-line selection))))

(handler/defhandler :delete-next-word :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (changes! selection)
    (let [np (caret selection)
          doc (text selection)
          next-word-move (next-word-move doc np)
          next-word-pos (+ np (count next-word-move))
          new-doc (str (subs doc 0 np)
                       (subs doc next-word-pos))]
      (text! selection new-doc))))

(handler/defhandler :delete-to-end-of-line :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (changes! selection)
    (let [np (caret selection)
          doc (text selection)
          line-end-offset (line-end-pos selection)
          new-doc (str (subs doc 0 np)
                       (subs doc line-end-offset))]
      (text! selection new-doc))))

(handler/defhandler :cut-to-end-of-line :code-view
  (enabled? [selection] (editable? selection))
  (run [selection clipboard]
    (changes! selection)
    (let [np (caret selection)
          doc (text selection)
          line-end-offset (line-end-pos selection)
          consume-pos (if (= line-end-offset np) (adjust-bounds doc (inc line-end-offset)) line-end-offset)
          new-doc (str (subs doc 0 np)
                       (subs doc consume-pos))
          clipboard-text (if (= :cut-to-end-of-line @last-command)
                           (str (text clipboard) (subs doc np consume-pos))
                           (subs doc np consume-pos))]
      (text! clipboard clipboard-text)
      (text! selection new-doc))))

(handler/defhandler :delete-to-start-of-line :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (changes! selection)
    (let [np (caret selection)
          doc (text selection)
          line-begin-offset (line-offset selection)
          new-doc (str (subs doc 0 line-begin-offset)
                       (subs doc np))]
      (text! selection new-doc)
      (caret! selection line-begin-offset false))))

(defn select-found-text [selection doc found-idx tlen]
  (when (<= 0 found-idx)
    (caret! selection (adjust-bounds doc (+ found-idx tlen)) false)
    (show-line selection)
    (text-selection! selection found-idx tlen)))

(defn find-text [selection find-text]
  (let [doc (text selection)
        found-idx (.indexOf ^String doc ^String find-text)
        tlen (count find-text)]
    (select-found-text selection doc found-idx tlen)))

(handler/defhandler :find-text :code-view
  (enabled? [selection] selection)
  (run [selection]
    ;; when using show and wait on the dialog, was getting very
    ;; strange double events not solved by consume - this is a
    ;; workaround to let us use show
    (let [text (promise)
          ^Stage stage (dialogs/make-find-text-dialog text)
          find-text-fn (fn [] (when (realized? text)
                                (find-text selection (or @text ""))))]
      (.setOnHidden stage (ui/event-handler e (find-text-fn))))))

(handler/defhandler :find-next :code-view
  (enabled? [selection] selection)
  (run [selection]
    (let [np (caret selection)
          doc (text selection)
          search-text (text-selection selection)
          found-idx (.indexOf ^String doc ^String search-text ^int np)
          tlen (count search-text)]
      (select-found-text selection doc found-idx tlen))))

(handler/defhandler :find-prev :code-view
  (enabled? [selection] selection)
  (run [selection]
    (let [np (caret selection)
          doc (text selection)
          search-text (text-selection selection)
          tlen (count search-text)
          found-idx (.lastIndexOf ^String doc ^String search-text ^int (adjust-bounds doc (- np (inc tlen))))]
      (select-found-text selection doc found-idx tlen))))

(defn do-replace [selection doc found-idx rtext tlen tlen-new caret-pos]
  (when (<= 0 found-idx)
    (replace! selection found-idx tlen rtext)
    (caret! selection (adjust-bounds (text selection) (+ tlen-new found-idx)) false)
    (show-line selection)
    (text-selection! selection found-idx tlen-new)
    ;;Note:  trying to highlight the selection doensn't
    ;; work quite right due to rendering problems in the StyledTextSkin
    ))

(defn replace-text [selection {ftext :find-text rtext :replace-text :as result}]
  (when (and ftext rtext)
    (changes! selection)
    (let [doc (text selection)
          found-idx (.indexOf ^String doc ^String ftext)
          tlen (count ftext)
          tlen-new (count rtext)]
      (do-replace selection doc found-idx rtext tlen tlen-new 0)
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
          replace-text-fn (fn [] (when (realized? result)
                                (replace-text selection (or @result {}))))]
      (.setOnHidden stage (ui/event-handler e (replace-text-fn))))))

(handler/defhandler :replace-next :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (changes! selection)
    (let [np (caret selection)
          doc (text selection)
          ftext @last-find-text
          found-idx (string/index-of doc ftext np)
          tlen (count ftext)
          rtext @last-replace-text
          tlen-new (count rtext)]
      (when found-idx
        (do-replace selection doc found-idx rtext tlen tlen-new np)))))

(defn add-comment [selection text line-comment]
  (str line-comment text))

(defn remove-comment [selection text line-comment]
  (subs text (count line-comment)))

(defn- syntax [source-viewer]
  (ui/user-data source-viewer :editor.code-view/syntax))

(defn toggle-line-comment [selection]
  (let [np (caret selection)
        doc (text selection)
        line-comment (:line-comment (syntax selection))
        line-text (line selection)
        loffset (line-offset selection)]
    (replace! selection loffset (count line-text) (toggle-comment line-text line-comment))))

(defn alter-line [selection line-num f & args]
  (let [current-line (line-at-num selection line-num)
        line-offset (line-offset-at-num selection line-num)
        new-text (apply f selection current-line args)]
    (when new-text
      (replace! selection (adjust-bounds (text selection) line-offset) (count current-line) new-text))))

(defn comment-line [selection line-comment line-num]
  (alter-line selection line-num add-comment line-comment))

(defn uncomment-line [selection line-comment line-num]
  (alter-line selection line-num remove-comment line-comment))

(defn do-toggle-line [selection line-comment line-num]
  (let [current-line (line-at-num selection line-num)
        line-offset (line-offset-at-num selection line-num)
        toggle-text (toggle-comment current-line line-comment)]
    (when toggle-text
      (replace! selection (adjust-bounds (text selection) line-offset) (count current-line) toggle-text))))

(defn comment-region [selection]
  (let [line-comment (:line-comment (syntax selection))
        region-start (selection-offset selection)
        region-len (selection-length selection)
        region-end (+ region-start region-len)
        start-line-num (line-num-at-offset selection region-start)
        start-line-offset (line-offset-at-num selection start-line-num)
        end-line-num (line-num-at-offset selection region-end)
        end-line-offset (line-offset-at-num selection end-line-num)
        caret-offset (caret selection)
        caret-line-num (line-num-at-offset selection caret-offset)]
    (doseq [i (range start-line-num (inc end-line-num))]
      (comment-line selection line-comment i))
    (let [line-comment-len (count line-comment)
          caret-offset-delta (* line-comment-len (inc (- caret-line-num start-line-num)))]
     (caret! selection (+ caret-offset-delta caret-offset) false)
     (text-selection! selection (+ line-comment-len region-start)
                      (+ region-len (* line-comment-len (- end-line-num start-line-num)))))))

(defn uncomment-region [selection]
  (let [line-comment (:line-comment (syntax selection))
        region-start (selection-offset selection)
        region-len (selection-length selection)
        region-end (+ region-start region-len)
        start-line-num (line-num-at-offset selection region-start)
        start-line-offset (line-offset-at-num selection start-line-num)
        end-line-num (line-num-at-offset selection region-end)
        end-line-offset (line-offset-at-num selection end-line-num)
        caret-offset (caret selection)
        caret-line-num (line-num-at-offset selection caret-offset)]
    (doseq [i (range start-line-num (inc end-line-num))]
      (uncomment-line selection line-comment i))
    (let [line-comment-len (count line-comment)
          caret-offset-delta (* line-comment-len (inc (- caret-line-num start-line-num)))]
      (caret! selection (- caret-offset caret-offset-delta) false)
      (text-selection! selection (- region-start line-comment-len)
                      (- region-len (* line-comment-len (- end-line-num start-line-num)))))))

(defn selected-lines [selection]
  (let [region-start (selection-offset selection)
        region-end (+ region-start (selection-length selection))
        start-line-num (line-num-at-offset selection region-start)
        end-line-num (line-num-at-offset selection region-end)]
    (for [line-num (range start-line-num (inc end-line-num))]
      (line-at-num selection line-num))))

(defn commented? [syntax s] (string/starts-with? s (:line-comment syntax)))

(handler/defhandler :toggle-comment :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (changes! selection)
    (if (pos? (count (text-selection selection)))
      (if (every? #(commented? (syntax selection) %) (selected-lines selection))
        (uncomment-region selection)
        (comment-region selection))
      (toggle-line-comment selection))))

(defn enter-key-text [selection key-typed]
  (let [np (caret selection)
        doc (text selection)]
    (if (pos? (selection-length selection))
      (replace-text-selection selection key-typed)
      (replace-text-and-caret selection np 0 key-typed (+ np (count key-typed))))))

(defn next-tab-trigger [selection pos]
  (when (has-snippet-tab-trigger? selection)
    (let [doc (text selection)
          tab-trigger-info (next-snippet-tab-trigger! selection)
          search-text (:trigger tab-trigger-info)
          exit-text (:exit tab-trigger-info)]
      (if (= :end search-text)
        (do
          (text-selection! selection pos 0)
          (if exit-text
            (let [found-idx (string/index-of doc exit-text pos)
                  tlen (count exit-text)]
              (when found-idx
                (caret! selection (+ found-idx tlen) false)
                (show-line selection)))
            (right selection)))
        (let [found-idx (string/index-of doc search-text pos)
              tlen (count search-text)]
          (when found-idx
            (select-found-text selection doc found-idx tlen)))))))

(defn prev-tab-trigger [selection pos]
  (let [doc (text selection)
        tab-trigger-info (prev-snippet-tab-trigger! selection)
        search-text (:trigger tab-trigger-info)]
    (when-not (= :begin search-text)
      (let [found-idx (string/last-index-of doc search-text pos)
            tlen (count search-text)]
        (when found-idx
          (select-found-text selection doc found-idx tlen))))))

(handler/defhandler :tab :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (changes! selection)
    (when (editable? selection)
      (if (has-snippet-tab-trigger? selection)
        (let [np (caret selection)
              doc (text selection)]
          (next-tab-trigger selection np))
        (enter-key-text selection "\t")))))

(handler/defhandler :backwards-tab-trigger :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (changes! selection)
    (when (editable? selection)
      (if (has-prev-snippet-tab-trigger? selection)
        (let [np (caret selection)
              doc (text selection)]
          (prev-tab-trigger selection np))))))

(defn- get-indentation [line]
  (re-find #"^\s+" line))

(defn indent-line [selection line line-above]
  (when-let [indentation-data (:indentation (syntax selection))]
    (let [{:keys [indent-chars increase? decrease?]} indentation-data
          current-indentation (get-indentation line)]
      (cond
        (decrease? line)
        (let [line-above-indent (or (get-indentation line-above) "")
              new-indent (string/replace-first line-above-indent (re-pattern indent-chars) "")
              line-without-indent (string/replace-first line (re-pattern (str indent-chars "*")) "")]
          (str new-indent line-without-indent))

        (increase? line-above)
        (let [line-above-indent (get-indentation line-above)
              line-without-indent (string/replace-first line (re-pattern (str indent-chars "*")) "")]
          (str line-above-indent indent-chars line-without-indent))

        :else
        (let [line-above-indent (get-indentation line-above)
              line-without-indent (string/replace-first line (re-pattern (str indent-chars "*")) "")
              line-above-increase? (increase? line-above)
              new-indent (if line-above-increase? (str line-above-indent indent-chars) line-above-indent)]
          (str new-indent line-without-indent))))))

(defn do-indent-line [selection line-num]
  (let [current-line (line-at-num selection line-num)
        line-offset (line-offset-at-num selection line-num)
        prev-line (if (= 0 line-num) "" (line-at-num selection (dec line-num)))
        indent-text (indent-line selection current-line prev-line)]
    (when indent-text
      (replace! selection (adjust-bounds (text selection) line-offset) (count current-line) indent-text))))

(defn do-indent-region [selection region-start region-end]
  (let [doc (text selection)
        start-line-num (line-num-at-offset selection region-start)
        start-line-offset (line-offset-at-num selection start-line-num)
        end-line-num (line-num-at-offset selection region-end)
        end-line-offset (line-offset-at-num selection end-line-num)]
    (doseq [i (range start-line-num (inc end-line-num))]
      (do-indent-line selection i))))

(handler/defhandler :indent :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (changes! selection)
    (when (editable? selection)
      (if (pos? (count (text-selection selection)))
        (let [region-start (selection-offset selection)
              region-len (selection-length selection)
              region-end (+ region-start region-len)]
         (do-indent-region selection region-start region-end))
        (let [line-num (line-num-at-offset selection (caret selection))]
         (do-indent-line selection line-num))))))

(handler/defhandler :enter :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (changes! selection)
    (when (editable? selection)
      (clear-snippet-tab-triggers! selection)
      (if (= (caret selection) (line-end-pos selection))
        (do
          (do-indent-line selection (line-num-at-offset selection (caret selection)))
          (enter-key-text selection (System/getProperty "line.separator"))
          (do-indent-line selection (line-num-at-offset selection (caret selection)))
          (caret! selection (+ (line-offset selection) (count (line selection))) false)
          (show-line selection))
        (enter-key-text selection (System/getProperty "line.separator"))))))

(handler/defhandler :undo :code-view
  (enabled? [selection] selection)
  (run [view-node code-node]
    (g/undo! (g/node-id->graph-id code-node))
    (g/node-value view-node :new-content)))

(handler/defhandler :redo :code-view
  (enabled? [selection] selection)
  (run [view-node code-node]
    (g/redo! (g/node-id->graph-id code-node))
    (g/node-value view-node :new-content)))

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

(defn do-proposal-replacement [selection replacement]
  (let [tab-triggers (:tab-triggers replacement)
        replacement (:insert-string replacement)
        offset (caret selection)
        line-text (line selection)
        loffset (line-offset selection)
        pattern (completion-pattern replacement loffset line-text offset)
        search-start (adjust-bounds (text selection) (- offset loffset (count pattern)))
        replacement-start (string/index-of line-text pattern search-start)
        replacement-len (count pattern)]
    (when replacement-start
      (replace! selection (+ loffset replacement-start) replacement-len replacement)
      (do-indent-region selection loffset (+ loffset (count replacement)))
      (snippet-tab-triggers! selection tab-triggers)
      (if (has-snippet-tab-trigger? selection)
        (let [nl (line selection)
              snippet-tab-start (or (when-let [start (:start tab-triggers)]
                                      (string/index-of nl start replacement-start))
                                    (string/index-of nl "(" replacement-start)
                                    replacement-start)]
          (next-tab-trigger selection (+ loffset snippet-tab-start)))
        (do  (caret! selection (+ loffset replacement-start (count replacement)) false)
             (show-line selection))))))

(defn show-proposals [selection proposals]
  (when (pos? (count proposals))
    (let [screen-position (screen-position selection)
          offset (caret selection)
          result (promise)
          current-line (line selection)
          target (completion-pattern nil (line-offset selection) current-line offset)
          ^Stage stage (dialogs/make-proposal-dialog result screen-position proposals target (text-area selection))
          replace-text-fn (fn [] (when (and (realized? result) @result)
                                  (do-proposal-replacement selection (first @result))))]
      (.setOnHidden stage (ui/event-handler e (replace-text-fn))))))

(handler/defhandler :proposals :code-view
  (enabled? [selection] selection)
  (run [selection]
    (let [proposals (propose selection)]
      (if (= 1 (count proposals))
        (let [replacement (first proposals)]
          (do-proposal-replacement selection replacement))
        (show-proposals selection proposals)))))

(defn match-key-typed [selection key-typed]
  (let [np (caret selection)
        match (get auto-matches key-typed)]
       (replace-text-and-caret selection np 0 match np)))

(defn- part-of-string? [selection]
  (let [line-text (line selection)
        loffset (line-offset selection)
        np (caret selection)
        check-text (subs line-text 0 (- np loffset))]
    (re-find  #"^(\w|\.)+\"" (->> check-text reverse (apply str)))))

(handler/defhandler :key-typed :code-view
  (enabled? [selection] (editable? selection))
  (run [selection key-typed]
    (when (and (editable? selection)
           (pos? (count key-typed))
           (code/not-ascii-or-delete key-typed))
      (enter-key-text selection key-typed)
      (when (and (contains? proposal-key-triggers key-typed) (not (part-of-string? selection)))
        (show-proposals selection (propose selection)))
      (when (contains? auto-match-set key-typed)
        (match-key-typed selection key-typed)))))
