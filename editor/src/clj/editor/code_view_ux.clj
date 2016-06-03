(ns editor.code-view-ux
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
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

(defprotocol TextView
  (text-selection [this])
  (text-selection! [this offset length])
  (selection-offset [this])
  (selection-length [this])
  (caret [this])
  (caret! [this offset select?])
  (editable? [this])
  (editable! [this val]))

(defprotocol TextStyles
  (styles [this]))

(defprotocol TextUndo
  (changes! [this]))

(def mappings
  {
  ;;;movement
  :Up                    {:command :up}
  :Down                  {:command :down}
  :Left                  {:command :left}
  :Right                 {:command :right}

  :Ctrl+Right            {:command :next-word               :label "Move to Next Word"               :group "Movement" :order 1}
  :Alt+Right             {:command :next-word}
  :Ctrl+Left             {:command :prev-word               :label "Move to Prev Word"               :group "Movement" :order 2}
  :Alt+Left              {:command :prev-word}
  :Shortcut+Left         {:command :line-begin              :label "Move to Line Begin"              :group "Movement" :order 3}
  :Ctrl+A                {:command :line-begin}
  :Shortcut+Right        {:command :line-end                :label "Move to Line End"                :group "Movement" :order 4}
  :Ctrl+E                {:command :line-end}
  :Shortcut+Up           {:command :file-begin              :label "Move to File Begin"              :group "Movement" :order 5}
  :Shortcut+Down         {:command :file-end                :label "Move to File End"                :group "Movement" :order 6}
  :Shortcut+L            {:command :goto-line               :label "Go to Line"                      :group "Movement" :order 7}

  ;; select
  :Double-Click          {:command :select-word}
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
  :Delete                {:command :delete}
  :Shortcut+D            {:command :cut                     :label "Delete Line"                     :group "Delete" :order 1}
  :Alt+Delete            {:command :delete-next-word        :label "Delete Next Word"                :group "Delete" :order 2}
  :Alt+Backspace         {:command :delete-prev-word        :label "Delete Prev Word"                :group "Delete" :order 3}
  :Shortcut+Delete       {:command :delete-to-start-of-line :label "Delete to the Start of the Line" :group "Delete" :order 4}
  :Shift+Shortcut+Delete {:command :delete-to-end-of-line   :label "Delete to the End of the Line"   :group "Delete" :order 5}

  ;; Comment
  :Shortcut+Slash        {:command :toggle-comment           :label "Toggle Comment"                  :group "Comment" :order 1}

  ;; Editing
  :Tab                   {:command :tab}
  :Enter                 {:command :enter}

})

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
        comment-commands (filter (fn [[k v]] (= "Comment" (:group v))) mappings)]
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
                  (sort-by :order (map menu-data comment-commands)))}]))

(def tab-size 4)
(def last-find-text (atom ""))
(def last-replace-text (atom ""))
(def prefer-offset (atom 0))

(defn preferred-offset [] @prefer-offset)
(defn preferred-offset! [val] (reset! prefer-offset val))

(defn- info [e]
  {:event e
   :key-code (.getCode ^KeyEvent e)
   :control? (.isControlDown ^KeyEvent e)
   :meta? (.isShortcutDown ^KeyEvent e)
   :alt? (.isAltDown ^KeyEvent e)
   :shift? (.isShiftDown ^KeyEvent e)})

(defn- add-modifier [info modifier-key modifier-str code-str]
  (if (get info modifier-key) (str modifier-str code-str) code-str))

;; potentially slow with each new keyword that is created
(defn- key-fn [info code]
  (let [code (->> (.getName ^KeyCode code)
                 (add-modifier info :meta? "Shortcut+")
                 (add-modifier info :alt? "Alt+")
                 (add-modifier info :control? "Ctrl+")
                 (add-modifier info :shift? "Shift+")
                 (keyword))]
    (get mappings code)))

(defn- click-fn [click-count]
  (let [code (if (= click-count 2) :Double-Click :Single-Click)]
    (get mappings code)))

(defn- is-mac-os? []
  (= "Mac OS X" (System/getProperty "os.name")))

(defn handle-key-pressed [^KeyEvent e source-viewer]
  (let [k-info (info e)
        kf (key-fn k-info (.getCode e))]

    (when (not (.isConsumed e))
      (.consume e)
      (when (and (:command kf) (not (:label kf)))
        (handler/run
          (:command kf)
          [{:name :code-view :env {:selection source-viewer :clipboard (Clipboard/getSystemClipboard)}}]
          k-info)
        (changes! source-viewer)))))

(defn is-not-typable-modifier? [e]
  (if (or (.isControlDown ^KeyEvent e) (.isAltDown ^KeyEvent e) (and (is-mac-os?) (.isMetaDown ^KeyEvent e)))
    (not (or (.isControlDown ^KeyEvent e) (and (is-mac-os?) (.isAltDown ^KeyEvent e))))))

(defn handle-key-typed [^KeyEvent e source-viewer]
  (let [key-typed (.getCharacter e)]
    (when-not (is-not-typable-modifier? e)
      (handler/run
        :key-typed
        [{:name :code-view :env {:selection source-viewer :key-typed key-typed}}]
        e)
      (changes! source-viewer))))


(defn- adjust-bounds [s pos]
  (if (neg? pos) 0 (min (count s) pos)))

(defn tab-count [s]
  (let [tab-count (count (filter #(= \tab %) s))]
    tab-count))

(defn lines-before [s pos]
  (let [np (adjust-bounds s pos)]
    ;; an extra char is put in to pick up mult newlines
    (let [lines (string/split (str (subs s 0 np) "x") #"\n")
          end (->> lines last drop-last (apply str))]
      (conj (vec (butlast lines)) end))))

(defn remember-caret-col [selection np]
  (let [lbefore (lines-before (text selection) np)
        text-before (->> (last lbefore) (apply str))
        tab-count (tab-count text-before)
        caret-col (+ (count text-before) (* tab-count (dec tab-size)))]
    (preferred-offset! caret-col)))

(defn handle-mouse-clicked [^MouseEvent e source-viewer]
  (let [click-count (.getClickCount e)
        cf (click-fn click-count)
        pos (caret source-viewer)]
    (remember-caret-col source-viewer pos)
    (when cf (handler/run
               (:command cf)
               [{:name :code-view :env {:selection source-viewer :clipboard (Clipboard/getSystemClipboard)}}]
               e))
    (changes! source-viewer)
    (.consume e)))

(defn- replace-text-selection [selection s]
  (replace! selection (selection-offset selection) (selection-length selection) s)
  (caret! selection (adjust-bounds (text selection) (selection-offset selection)) false))

(defn- replace-text-and-caret [selection offset length s new-caret-pos]
  (replace! selection (adjust-bounds (text selection) offset) length s)
  (caret! selection (adjust-bounds (text selection) new-caret-pos) false)
  (remember-caret-col selection new-caret-pos))

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
      (caret! selection next-pos false)
      (remember-caret-col selection next-pos)))

(defn select-right [selection]
  (let [c (caret selection)
        doc (text selection)
        selected-text (text-selection selection)
        next-pos (adjust-bounds doc (inc c))]
      (caret! selection next-pos true)
      (remember-caret-col selection next-pos)))

(defn left [selection]
  (let [c (caret selection)
        doc (text selection)
        selected-text (text-selection selection)
        next-pos (if (pos? (count selected-text))
                   (adjust-bounds doc (- c (count selected-text)))
                   (adjust-bounds doc (dec c)))]
      (caret! selection next-pos false)
      (remember-caret-col selection next-pos)))

(defn select-left [selection]
  (let [c (caret selection)
        doc (text selection)
        selected-text (text-selection selection)
        next-pos (adjust-bounds doc (dec c))]
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

(defn lines-after [s pos]
  (let [np (adjust-bounds s pos)]
    (string/split (subs s np) #"\n")))

(defn up-line [s pos preferred-offset]
  (let [lines-before (lines-before s pos)
        len-before (-> lines-before last count)
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

(defn up [selection]
  (let [c (caret selection)
        doc (text selection)
        preferred-offset (preferred-offset)
        next-pos (if (pos? (selection-length selection))
                   (adjust-bounds doc (selection-offset selection))
                   (up-line doc c preferred-offset))]
    (caret! selection next-pos false)))

(defn select-up [selection]
  (let [c (caret selection)
        doc (text selection)
        preferred-offset (preferred-offset)
        next-pos (up-line doc c preferred-offset)]
    (caret! selection next-pos true)))

(defn down [selection]
  (let [c (caret selection)
        doc (text selection)
        preferred-offset (preferred-offset)
        next-pos (if (pos? (selection-length selection))
                   (adjust-bounds doc (+ (selection-offset selection)
                                         (selection-length selection)))
                   (down-line doc c preferred-offset))]
      (caret! selection next-pos false)))

(defn select-down [selection]
  (let [c (caret selection)
        doc (text selection)
        preferred-offset (preferred-offset)
        next-pos (down-line doc c preferred-offset)]
      (caret! selection next-pos true)))

(handler/defhandler :up :code-view
  (enabled? [selection] selection)
  (run [selection]
    (up selection)))

(handler/defhandler :down :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (down selection)))

(handler/defhandler :select-up :code-view
  (enabled? [selection] selection)
  (run [selection]
    (select-up selection)))

(handler/defhandler :select-down :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (select-down selection)))

(def word-regex  #"\n|\s*_*[a-zA-Z0-9]+|.")

(defn next-word-move [doc np]
  (re-find word-regex (subs doc np)))

(defn next-word [selection select?]
  (let [c (caret selection)
        doc (text selection)
        np (adjust-bounds doc c)
        next-word-move (next-word-move doc np)
        next-pos (+ np (count next-word-move))]
    (caret! selection next-pos select?)
    (remember-caret-col selection next-pos)))

(defn prev-word-move [doc np]
  (re-find word-regex (->> (subs doc 0 np) (reverse) (apply str))))

(defn prev-word [selection select?]
  (let [c (caret selection)
        doc (text selection)
        np (adjust-bounds doc c)
        next-word-move (prev-word-move doc np)
        next-pos (- np (count next-word-move))]
    (caret! selection next-pos select?)
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

(defn line-begin-pos [selection]
  (let [c (caret selection)
        doc (text selection)
        np (adjust-bounds doc c)
        lines-before (lines-before doc np)
        len-before (-> lines-before last count)]
        (- np len-before)))

(defn line-begin [selection select?]
  (let [next-pos (line-begin-pos selection)]
    (caret! selection next-pos select?)
    (remember-caret-col selection next-pos)))

(defn line-end-pos [selection]
  (let [c (caret selection)
        doc (text selection)
        np (adjust-bounds doc c)
        lines-after (lines-after doc np)
        len-after (-> lines-after first count)]
    (+ np len-after)))

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
    (caret! selection 0 false)))

(handler/defhandler :file-end :code-view
  (enabled? [selection] selection)
  (run [selection]
    (let [doc (text selection)]
      (caret! selection (count doc) false))))

(handler/defhandler :select-file-begin :code-view
  (enabled? [selection] selection)
  (run [selection]
    (caret! selection 0 true)))

(handler/defhandler :select-file-end :code-view
  (enabled? [selection] selection)
  (run [selection user-data]
    (let [doc (text selection)]
      (caret! selection (count doc) true))))

(defn go-to-line [selection line-number]
  (try
    (let [line  (Integer/parseInt line-number)
          doc (text selection)
          doc-lines (string/split doc #"\n")
          target-lines (take (dec line) doc-lines)
          np (+ (count target-lines) (reduce + (map count target-lines)))]
      (caret! selection (adjust-bounds doc (if (zero? np) 0 np)) false))
    (catch Exception e (println "Not a valid line number" line-number (.getMessage e)))))

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
          c (caret selection)
          doc (text selection)
          np (adjust-bounds doc c)
          word-end (re-find regex (subs doc np))
          text-before (->> (subs doc 0 (inc np)) (reverse) (rest) (apply str))
          word-begin (re-find regex text-before)
          start-pos (- np (count word-begin))
          end-pos (+ np (count word-end))
          len (- end-pos start-pos)]
      (text-selection! selection start-pos len))))

(handler/defhandler :select-all :code-view
  (enabled? [selection] selection)
  (run [selection]
    (text-selection! selection 0 (count (text selection)))))

(defn delete [selection]
  (let [c (caret selection)
        doc (text selection)
        np (adjust-bounds doc c)]
    (if (pos? (selection-length selection))
      (replace-text-selection selection "")
      (when-not (zero? np)
       (replace-text-and-caret selection (dec np) 1 "" (dec np))))))

(handler/defhandler :delete :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (when (editable? selection)
      (delete selection))))

(defn cut-selection [selection clipboard]
  (text! clipboard (text-selection selection))
  (replace-text-selection selection ""))

(defn cut-line [selection]
  (let [c (caret selection)
        doc (text selection)
        np (adjust-bounds doc c)
        line-begin-offset (line-begin-pos selection)
        line-end-offset (line-end-pos selection)
        consume-pos (if (= line-end-offset (count doc))
                      line-end-offset
                      (inc line-end-offset))
        new-doc (str (subs doc 0 line-begin-offset)
                     (subs doc consume-pos))]
    (text! selection new-doc)
    (caret! selection (adjust-bounds new-doc line-begin-offset) false)))

(handler/defhandler :cut :code-view
  (enabled? [selection] (editable? selection))
  (run [selection clipboard]
    (if (pos? (selection-length selection))
      (cut-selection selection clipboard)
      (cut-line selection))))

(handler/defhandler :delete-prev-word :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (let [c (caret selection)
          doc (text selection)
          np (adjust-bounds doc c)
          next-word-move (prev-word-move doc np)
          new-pos (- np (count next-word-move))
          new-doc (str (subs doc 0 new-pos)
                       (subs doc np))]
      (text! selection new-doc)
      (caret! selection new-pos false))))

(handler/defhandler :delete-next-word :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (let [c (caret selection)
          doc (text selection)
          np (adjust-bounds doc c)
          next-word-move (next-word-move doc np)
          next-word-pos (+ np (count next-word-move))
          new-doc (str (subs doc 0 np)
                       (subs doc next-word-pos))]
      (text! selection new-doc))))

(handler/defhandler :delete-to-end-of-line :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (let [c (caret selection)
          doc (text selection)
          np (adjust-bounds doc c)
          line-end-offset (line-end-pos selection)
          new-doc (str (subs doc 0 np)
                       (subs doc line-end-offset))]
      (text! selection new-doc))))

(handler/defhandler :delete-to-start-of-line :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (let [c (caret selection)
          doc (text selection)
          np (adjust-bounds doc c)
          line-begin-offset (line-begin-pos selection)
          new-doc (str (subs doc 0 line-begin-offset)
                       (subs doc np))]
      (text! selection new-doc)
      (caret! selection line-begin-offset false))))

(defn select-found-text [selection doc found-idx tlen]
  (when (<= 0 found-idx)
    (caret! selection (adjust-bounds doc (+ found-idx tlen)) false)
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
                                (find-text selection @text)))]
      (.setOnHidden stage (ui/event-handler e (find-text-fn))))))

(handler/defhandler :find-next :code-view
  (enabled? [selection] selection)
  (run [selection]
    (let [c (caret selection)
          doc (text selection)
          np (adjust-bounds doc c)
          search-text (text-selection selection)
          found-idx (.indexOf ^String doc ^String search-text ^int np)
          tlen (count search-text)]
      (select-found-text selection doc found-idx tlen))))

(handler/defhandler :find-prev :code-view
  (enabled? [selection] selection)
  (run [selection]
    (let [c (caret selection)
          doc (text selection)
          np (adjust-bounds doc c)
          search-text (text-selection selection)
          tlen (count search-text)
          found-idx (.lastIndexOf ^String doc ^String search-text ^int (adjust-bounds doc (- np (inc tlen))))]
      (select-found-text selection doc found-idx tlen))))

(defn do-replace [selection doc found-idx rtext tlen tlen-new caret-pos]
  (when (<= 0 found-idx)
    (replace! selection found-idx tlen rtext)
    (caret! selection (adjust-bounds (text selection) (+ tlen-new found-idx)) false)
    ;; (text-selection! selection found-idx tlen-new)
    ;;Note:  trying to highlight the selection doensn't
    ;; work due to rendering problems in the StyledTextSkin
    ))

(defn replace-text [selection {ftext :find-text rtext :replace-text :as result}]
  (when (and ftext rtext)
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
                                (replace-text selection @result)))]
      (.setOnHidden stage (ui/event-handler e (replace-text-fn))))))

(handler/defhandler :replace-next :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (let [c (caret selection)
          doc (text selection)
          np (adjust-bounds doc c)
          ftext @last-find-text
          found-idx (.indexOf ^String doc ^String ftext ^int np)
          tlen (count ftext)
          rtext @last-replace-text
          tlen-new (count rtext)]
      (do-replace selection doc found-idx rtext tlen tlen-new np))))

(defn toggle-comment [text line-comment]
  (let [pattern (re-pattern (str "^" line-comment))]
   (if (re-find pattern text)
     (->> text (drop (count line-comment)) (apply str))
     (str line-comment text))))

(defn- syntax [source-viewer]
  (ui/user-data source-viewer :editor.code-view/syntax))

(defn do-toggle-line [doc lbefore lafter line-comment]
  (let [text-after (first lafter)
        text-before (->> (last lbefore) (apply str))]
    (toggle-comment (str text-before text-after) line-comment)))

(defn toggle-line-comment [selection]
  (let [c (caret selection)
        doc (text selection)
        np (adjust-bounds doc c)
        line-comment (:line-comment (syntax selection))
        lbefore (lines-before doc np)
        lafter (lines-after doc np)
        toggled-text (do-toggle-line doc lbefore lafter line-comment)
        new-lines (concat (butlast lbefore)
                          [toggled-text]
                          (rest lafter))
        new-doc (apply str (interpose "\n" new-lines))]
    (text! selection new-doc)))

(defn toggle-region-comment [selection]
  (let [c (caret selection)
        doc (text selection)
        line-comment (:line-comment (syntax selection))
        region-start (selection-offset selection)
        region-len (selection-length selection)
        region-end (+ region-start region-len)
        lbefore (lines-before doc region-start)
        lafter (lines-after doc region-end)
        region-lines (string/split (text-selection selection) #"\n")
        toggled-region-lines (map #(toggle-comment % line-comment) (-> region-lines rest butlast))
        first-toggled-line (do-toggle-line doc lbefore (lines-after doc region-start) line-comment)
        last-toggled-line (do-toggle-line doc (lines-before doc region-end) lafter line-comment)
        new-lines (flatten (conj []
                                 (butlast lbefore)
                                 [first-toggled-line]
                                 toggled-region-lines
                                 [last-toggled-line]
                                 (rest lafter)))
        new-doc (apply str (interpose "\n" new-lines))]
    (text! selection new-doc)))

(handler/defhandler :toggle-comment :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (if (pos? (count (text-selection selection)))
      (toggle-region-comment selection)
      (toggle-line-comment selection))))

(defn- not-ascii-or-delete [key-typed]
 ;; ascii control chars like Enter are all below 32
  ;; delete is an exception and is 127
  (let [n (.charAt ^String key-typed 0)]
    (and (> (int n) 31) (not= (int n) 127))))

(defn enter-key-text [selection key-typed]
  (let [c (caret selection)
        doc (text selection)
        np (adjust-bounds doc c)]
    (if (pos? (selection-length selection))
      (replace-text-selection selection key-typed)
      (replace-text-and-caret selection np 0 key-typed (inc np)))))

(handler/defhandler :key-typed :code-view
  (enabled? [selection] (editable? selection))
  (run [selection key-typed]
    (when (and (editable? selection)
           (pos? (count key-typed))
           (not-ascii-or-delete key-typed))
      (enter-key-text selection key-typed))))

(handler/defhandler :tab :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (when (editable? selection)
      (enter-key-text selection "\t"))))

(handler/defhandler :enter :code-view
  (enabled? [selection] (editable? selection))
  (run [selection]
    (when (editable? selection)
      (let [line-seperator (System/getProperty "line.separator")]
        (enter-key-text selection line-seperator)))))

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
