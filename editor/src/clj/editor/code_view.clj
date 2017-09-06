(ns editor.code-view
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [editor.code-view-ux :as cvx]
            [editor.core :as core]
            [editor.handler :as handler]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace])
  (:import [com.defold.editor.eclipse DefoldRuleBasedScanner Document DefoldStyledTextSkin DefoldStyledTextSkin$LineCell
            DefoldStyledTextBehavior DefoldStyledTextArea DefoldSourceViewer DefoldStyledTextLayoutContainer]
           [javafx.beans.property SimpleStringProperty]
           [javafx.scene Node Parent Cursor]
           [javafx.scene.layout GridPane Priority ColumnConstraints]
           [javafx.scene.shape Rectangle]
           [javafx.scene.input Clipboard ClipboardContent KeyEvent KeyCode MouseEvent]
           [javafx.scene.image Image ImageView]
           [java.util.function Function]
           [javafx.scene.control ListView ListCell Tab Label TextField CheckBox]
           [org.eclipse.fx.text.ui TextAttribute]
           [org.eclipse.fx.text.ui.presentation PresentationReconciler]
           [org.eclipse.fx.text.ui.rules DefaultDamagerRepairer]
           [org.eclipse.fx.text.ui.source SourceViewer SourceViewerConfiguration]
           [org.eclipse.fx.ui.controls.styledtext StyledTextArea StyleRange TextSelection StyledTextLayoutContainer]
           [org.eclipse.fx.ui.controls.styledtext.behavior StyledTextBehavior]
           [org.eclipse.fx.ui.controls.styledtext.skin StyledTextSkin]
           [org.eclipse.jface.text DocumentEvent IDocument IDocumentListener IDocumentPartitioner]
           [org.eclipse.jface.text.rules FastPartitioner ICharacterScanner IPredicateRule IRule IToken IWhitespaceDetector
            IWordDetector MultiLineRule RuleBasedScanner RuleBasedPartitionScanner SingleLineRule Token WhitespaceRule WordRule]))

(set! *warn-on-reflection* true)

(ui/extend-menu ::text-edit :editor.app-view/edit-end
                (cvx/create-menu-data))

(defn- code-node [text-area]
  (-> text-area
    (ui/user-data ::view-id)
    (g/node-value :resource-node)))

(defn- behavior [text-area]
  (ui/user-data text-area ::behavior))

(defn- assist [text-area]
  (ui/user-data text-area ::assist))

(defn- syntax [text-area]
  (ui/user-data text-area ::syntax))

(defn- prefer-offset [source-viewer]
  (ui/user-data source-viewer ::prefer-offset))

(defn- tab-triggers [source-viewer]
  (ui/user-data source-viewer ::tab-triggers))

(defn- typing-opseq [source-viewer]
  (ui/user-data source-viewer ::typing-opseq))

(defn- last-command-data [source-viewer]
  (ui/user-data source-viewer ::last-command))

(defn- new-typing-opseq! [^SourceViewer source-viewer merge-changes]
  (let [graph (g/node-id->graph-id (-> source-viewer (.getTextWidget) (code-node)))
        opseq (or (when merge-changes (when-let [typing-op @(typing-opseq source-viewer)]
                                        (when-let [last-op (g/prev-sequence-label graph)]
                                          (when (= typing-op last-op)
                                            typing-op))))
                  (gensym))]
    (reset! (typing-opseq source-viewer) opseq)
    opseq))
  
(defn typing-timeout [source-viewer]
  (ui/user-data source-viewer ::typing-timeout))

(defn- restart-typing-timeout [source-viewer]
  (if-let [timer @(typing-timeout source-viewer)]
    (ui/restart timer)
    (reset! (typing-timeout source-viewer)
            (ui/->future 1 (fn [] (reset! (typing-opseq source-viewer) nil))))))

(g/defnk update-source-viewer [_node-id ^SourceViewer source-viewer code caret-position selection-offset selection-length prefer-offset tab-triggers]
  (ui/user-data! (.getTextWidget source-viewer) ::view-id _node-id)
  (when code
    (let [did-update
          (some some?
            [(when (not= code (cvx/text source-viewer))
               (cvx/text! source-viewer code)
               :updated)
             (when (not= prefer-offset (cvx/preferred-offset source-viewer))
               (cvx/preferred-offset! source-viewer prefer-offset)
               :updated)
             (when (not= tab-triggers (cvx/snippet-tab-triggers source-viewer))
               (cvx/snippet-tab-triggers! source-viewer tab-triggers)
               :updated)
             (when (not= caret-position (cvx/caret source-viewer))
               (cvx/caret! source-viewer caret-position false)
               (cvx/show-line source-viewer)
               :updated)
             (when (not= [selection-offset selection-length]
                         [(cvx/selection-offset source-viewer) (cvx/selection-length source-viewer)])
               (cvx/caret! source-viewer caret-position false)
               (cvx/text-selection! source-viewer selection-offset selection-length)
               :updated)])]
      (when did-update
        (reset! (last-command-data source-viewer) nil))))
  source-viewer)

(defn- default-rule? [rule]
  (= (:type rule) :default))

(def ^:private attr-tokens (atom nil))
(def ^:private ^Character char0 (char 0))

(defn- attr-token [attr]
  (Token. (TextAttribute. attr)))

(defn get-attr-token [name]
  (if-let [token (get @attr-tokens name)]
    token
    (do
      (swap! attr-tokens assoc name (attr-token name))
      (get-attr-token name))))

(def ^:private tokens (atom nil))

(defn- token [name]
  (Token. name))

(defn- get-token [name]
  (if-let [token (get @tokens name)]
    token
    (do
      (swap! tokens assoc name (token name))
      (get-token name))))

(defmulti make-rule (fn [rule token] (:type rule)))

(defmethod make-rule :default [rule _]
  (assert false (str "default rule " rule " should be handled separately")))

(defn- white-space-detector [space?]
  (reify IWhitespaceDetector
    (isWhitespace [this c] (boolean (space? c)))))

(defn- is-space? [^Character c] (Character/isWhitespace c))

(defmethod make-rule :whitespace [{:keys [space?]} token]
  (let [space? (or space? is-space?)]
    (if token
      (WhitespaceRule. (white-space-detector space?) token)
      (WhitespaceRule. (white-space-detector space?)))))

(defn- word-detector [start? part?]
  (reify IWordDetector
    (isWordStart [this c] (boolean (start? c)))
    (isWordPart [this c] (boolean (part? c)))))

(defmethod make-rule :keyword [{:keys [start? part? keywords]} token]
  (let [word-rule (WordRule. (word-detector start? part?))]
    (doseq [keyword keywords]
      (.addWord word-rule keyword token))
    word-rule))

(defmethod make-rule :word [{:keys [start? part?]} token]
  (WordRule. (word-detector start? part?) token))

(defmethod make-rule :singleline [{:keys [start end esc]} token]
  (if esc
    (SingleLineRule. start end token esc)
    (SingleLineRule. start end token)))

(defmethod make-rule :multiline [{:keys [start end esc eof]} token]
  (MultiLineRule. start end token (if esc esc char0) (boolean eof)))

(defn- make-predicate-rule [scanner-fn ^IToken token]
  (reify IPredicateRule
    (evaluate [this scanner]
      (.evaluate this scanner false))
    (evaluate [this scanner resume]
      (let [^DefoldRuleBasedScanner sc scanner
            ch-seq (.readSequence sc)]
        (let [result (scanner-fn ch-seq)]
          (if (and result (pos? (:length result)))
            (do (.moveForward sc (:length result))
                token)
            Token/UNDEFINED))))
    (getSuccessToken ^IToken [this] token)))

(defmethod make-rule :custom [{:keys [scanner]} token]
  (make-predicate-rule scanner token))

(deftype NumberRule [^IToken token]
  IRule
  (^IToken evaluate [this ^ICharacterScanner scanner]
   (let [c (.read scanner)]
     (if (and (not= c ICharacterScanner/EOF) (Character/isDigit (char c)))
       (do
         (loop [c (.read scanner)]
           (when (and (not= c ICharacterScanner/EOF) (Character/isDigit (char c)))
             (recur (.read scanner))))
         (.unread scanner)
         token)
       (do
         (.unread scanner)
         Token/UNDEFINED)))))

(defmethod make-rule :number [_ token]
  (NumberRule. token))

(defn- make-scanner-rule [{:keys [class] :as rule}]
  (make-rule rule (when class (get-attr-token class))))

(defn- make-scanner [rules]
  (let [scanner (DefoldRuleBasedScanner.)
        default-rule (first (filter default-rule? rules))
        rules (remove default-rule? rules)]
    (when default-rule
      (.setDefaultReturnToken scanner (get-attr-token (:class default-rule))))
    (.setRules scanner (into-array IRule (map make-scanner-rule rules)))
    scanner))

(def ^:private default-content-type-map {:default IDocument/DEFAULT_CONTENT_TYPE})

(defn- make-multiline-dr [sc]
  (proxy [DefaultDamagerRepairer] [sc]
    (getDamageRegion [p e chg]
      p)))

(defn- make-reconciler [^SourceViewerConfiguration configuration ^SourceViewer source-viewer scanner-syntax]
  (let [pr (PresentationReconciler.)]
    (.setDocumentPartitioning pr (.getConfiguredDocumentPartitioning configuration source-viewer))
    (doseq [{:keys [partition rules]} scanner-syntax]
      (let [partition (get default-content-type-map partition partition)]
        (let [damager-repairer (make-multiline-dr (make-scanner rules))] ;;_(DefaultDamagerRepairer. (make-scanner rules))]
          (.setDamager pr damager-repairer partition)
          (.setRepairer pr damager-repairer partition))))
    pr))

(defn- ^SourceViewerConfiguration create-viewer-config [source-viewer opts]
  (let [{:keys [language syntax assist]} opts
        scanner-syntax (:scanner syntax)]
    (proxy [SourceViewerConfiguration] []
      (getStyleclassName [] language)
      (getPresentationReconciler [source-viewer]
        (make-reconciler this source-viewer scanner-syntax))
      (getConfiguredContentTypes [source-viewer]
        (into-array String (replace default-content-type-map (map :partition scanner-syntax)))))))

(defn- default-partition? [partition]
  (= (:type partition) :default))

(defn- make-partition-rule [rule]
  (make-rule rule (get-token (:partition rule))))

(defn- make-partition-scanner [partitions]
  (let [rules (map make-partition-rule (remove default-partition? partitions))]
    (doto (RuleBasedPartitionScanner.)
      (.setPredicateRules (into-array IPredicateRule rules)))))

(defn- ^IDocumentPartitioner make-partitioner [opts]
  (when-let [partitions (get-in opts [:syntax :scanner])]
    (let [legal-content-types (map :partition (remove default-partition? partitions))]
      (FastPartitioner. (make-partition-scanner partitions)
                        (into-array String legal-content-types)))))

(defprotocol TextPointer
  (caret-at-point [this x y]))

(extend-type ListCell
  TextPointer
  (caret-at-point [this x y]
    (let [bip (.getBoundsInParent this)
          de (.getDomainElement ^DefoldStyledTextSkin$LineCell this)
          ^DefoldStyledTextLayoutContainer n (.getGraphic this)
          base {:min-x (.getMinX bip)
                :min-y (.getMinY bip)
                :max-x (.getMaxX bip)
                :max-y (.getMaxY bip)}]
     (cond-> base

      de
      (merge {:line-offset (.getLineOffset de)
              :line-length (.getLineLength de)
              :line-index (.getLineIndex de)})

      n
       (merge {:caret-idx  (.getCaretIndexAtPoint n (.sceneToLocal n ^double x ^double y))
               :start-offset (.getStartOffset n)})))))

(extend-type StyledTextArea
  cvx/TextCaret
  (caret! [this offset select?]
    (try
      (.impl_setCaretOffset this offset select?)
      (catch Exception e
        (println "caret! failure")
        )))
  (caret [this] (.getCaretOffset this)))

(defn setup-source-viewer [opts]
  (let [source-viewer (DefoldSourceViewer.)
        clip-rectangle (Rectangle. 0 0)
        source-viewer-config (create-viewer-config source-viewer opts)
        document (Document. "")
        partitioner (make-partitioner opts)]

    (.setClip source-viewer clip-rectangle)
    (.bind (.heightProperty clip-rectangle) (.heightProperty source-viewer))
    (.bind (.widthProperty clip-rectangle) (.widthProperty source-viewer))

    (when partitioner
      (.setDocumentPartitioner document (.getConfiguredDocumentPartitioning source-viewer-config source-viewer) partitioner)
      (.connect partitioner document))

    (.configure source-viewer source-viewer-config)
    (.setDocument source-viewer document)

    (let [text-area (.getTextWidget source-viewer)
          styled-text-behavior  (proxy [DefoldStyledTextBehavior] [text-area]
                                  (callActionForEvent [key-event]
                                    ;;do nothing we are handling all the events
                                    )
                                  (defoldUpdateCursor [^MouseEvent event visible-cells selection line-label]
                                    ;;; ported behavior from the underlying Java class
                                    (try
                                      (let [vcells (into [] visible-cells)
                                           doc-len (.getCharCount ^StyledTextArea text-area)
                                           event-y (.getY event)
                                           event-x (.getX event)
                                           line-index (when line-label (.getText ^Label line-label))
                                           scene-event-x (.getSceneX event)
                                           scene-event-y (.getSceneY event)
                                           caret-cells (sort-by :min-y (mapv #(caret-at-point % scene-event-x scene-event-y) vcells))
                                           found-cells (filter #(when-let [idx (:caret-idx %)]
                                                                  (not= -1 idx)) caret-cells)
                                           found-cells2 (when line-label
                                                          (filter #(when-let [idx (:line-index %)]
                                                                     (when line-index (= line-index (str (inc idx))))) caret-cells))]

                                        (if-let [fcell (first found-cells)]
                                          (cvx/caret! text-area (+ (:start-offset fcell) (:caret-idx fcell)) selection)
                                          (let [closest-cell (some #(when (>= (:max-y %) event-y) %) caret-cells)
                                                found-cell (if (neg? event-y)
                                                             (first caret-cells)
                                                             (first found-cells2))
                                                fcell (or found-cell closest-cell)
                                                line-offset (:line-offset fcell)
                                                line-length (:line-length fcell)
                                                line-index (:line-index fcell)
                                                x-threshold 10]
                                            (cond

                                              ;;; we are drag selecting up
                                              (and (neg? event-y) line-index)
                                              (let [line-num (cvx/prev-line-num source-viewer)]
                                                (when (not (neg? line-num))
                                                  (cvx/caret! source-viewer (cvx/line-offset-at-num source-viewer line-num) selection)))

                                              ;; we are off to the left of the doc (in the gutter)
                                              (and line-offset line-label)
                                              (cvx/caret! text-area line-offset selection)

                                              ;; we are off to the left of the doc (but not in the gutter)
                                              (and line-offset (< event-x x-threshold))
                                              (cvx/caret! text-area line-offset selection)

                                              ;; we are off to the right of the doc
                                              line-offset
                                              (cvx/caret! text-area (+ line-offset line-length) selection)

                                              ;;; we are clicking below the end of the document (entire doc is in viewport)
                                              ;; and we want to jump to the end
                                              fcell
                                              (cvx/caret! text-area doc-len selection)

                                              ;; we are drag selecting down
                                              true
                                              (let [line-num (cvx/next-line-num source-viewer)]
                                                (when line-num
                                                  (cvx/caret! source-viewer (cvx/line-offset-at-num source-viewer line-num) selection)))))))
                                      (cvx/show-line source-viewer)
                                      (catch Exception e
                                        (println "error updating cursor" (.printStackTrace e))))))]
      (.addEventHandler ^StyledTextArea text-area
                        KeyEvent/KEY_PRESSED
                        (ui/event-handler e (cvx/handle-key-pressed e source-viewer)))
      (.addEventHandler ^StyledTextArea text-area
                        KeyEvent/KEY_TYPED
                        (ui/event-handler e (cvx/handle-key-typed e source-viewer)))
      (.setOnMouseEntered ^StyledTextArea text-area
                          (ui/event-handler e
                                            (.setCursor (.getScene text-area) Cursor/DEFAULT)))
     (let [skin (new DefoldStyledTextSkin text-area styled-text-behavior)]
       (.setSkin text-area skin)
       (.addEventHandler  ^ListView (.getListView skin)
                          MouseEvent/MOUSE_CLICKED
                          (ui/event-handler e (cvx/handle-mouse-clicked e source-viewer)))
       (.setMinWidth (.getLineRuler skin) 50)) ; make it fit reasonably large line numbers to avoid jumps when scrolling

      (ui/user-data! text-area ::behavior styled-text-behavior)
      (ui/user-data! source-viewer ::assist (:assist opts))
      (ui/user-data! source-viewer ::syntax (:syntax opts))
      (ui/user-data! source-viewer ::prefer-offset (atom 0))
      (ui/user-data! source-viewer ::tab-triggers (atom nil))
      (ui/user-data! source-viewer ::typing-opseq (atom nil))
      (ui/user-data! source-viewer ::typing-timeout (atom nil))
      (ui/user-data! source-viewer ::last-command (atom nil)))
    

  source-viewer))

(g/defnode CodeView
  (inherits view/WorkbenchView)

  (property source-viewer SourceViewer)

  (input code g/Str)
  (input caret-position g/Int)
  (input prefer-offset g/Int)
  (input tab-triggers g/Any)
  (input selection-offset g/Int)
  (input selection-length g/Int)
  (output new-content g/Any :cached update-source-viewer))

(defn setup-code-view [app-view-id view-id code-node initial-caret-position]
  (g/transact
   (concat
    (g/connect code-node :code view-id :code)
    (g/connect code-node :caret-position view-id :caret-position)
    (g/connect code-node :prefer-offset view-id :prefer-offset)
    (g/connect code-node :tab-triggers view-id :tab-triggers)
    (g/connect code-node :selection-offset view-id :selection-offset)
    (g/connect code-node :selection-length view-id :selection-length)
    (g/set-property code-node :caret-position initial-caret-position)
    (g/set-property code-node :prefer-offset 0)
    (g/set-property code-node :tab-triggers nil)
    (g/set-property code-node :selection-offset 0)
    (g/set-property code-node :selection-length 0)))
  view-id)

(extend-type Clipboard
  cvx/TextContainer
  (text! [this s]
    (let [content (ClipboardContent.)]
      (.putString content s)
      (.setContent this content)))
  (text [this]
    (when (.hasString this)
      (.getString this))))

(defn- caret-at-start-or-end-of-selection? [source-viewer offset]
  (and (pos? (cvx/selection-length source-viewer))
       (or (= offset (cvx/selection-offset source-viewer))
           (= offset (+ (cvx/selection-length source-viewer)
                        (cvx/selection-offset source-viewer))))))

(defn source-viewer-set-caret! [source-viewer offset select?]
  (let [select-value (and select? (not (caret-at-start-or-end-of-selection? source-viewer offset)))]
   (cvx/caret! (.getTextWidget ^SourceViewer source-viewer)
               (cvx/adjust-bounds (cvx/text source-viewer) offset)
               select-value)))

(declare skin)

(defn- transact-changes [^SourceViewer text-area opseq]
  (let [code-node-id (-> text-area (.getTextWidget) (code-node))
        selection-offset (cvx/selection-offset text-area)
        selection-length (cvx/selection-length text-area)
        code (cvx/text text-area)
        caret (cvx/caret text-area)
        prefer-offset (cvx/preferred-offset text-area)
        tab-triggers (cvx/snippet-tab-triggers text-area)
        code-changed? (not= code (g/node-value code-node-id :code))
        caret-changed? (not= caret (g/node-value code-node-id :caret-position))
        selection-changed? (or (not= selection-offset (g/node-value code-node-id :selection-offset))
                               (not= selection-length (g/node-value code-node-id :selection-length)))
        prefer-offset-changed? (not= prefer-offset (g/node-value code-node-id :prefer-offset))
        tab-triggers-changed? (not= tab-triggers (g/node-value code-node-id :tab-triggers))]
    (let [tx-data (cond-> []
                    code-changed?          (conj (g/set-property code-node-id :code code))
                    caret-changed?         (conj (g/set-property code-node-id :caret-position caret))
                    opseq                  (conj (g/operation-sequence opseq))
                    selection-changed?     (conj (g/set-property code-node-id :selection-offset selection-offset)
                                                 (g/set-property code-node-id :selection-length selection-length))
                    prefer-offset-changed? (conj (g/set-property code-node-id :prefer-offset prefer-offset))
                    tab-triggers-changed?  (conj (g/set-property code-node-id :tab-triggers tab-triggers)))]
      (when (seq tx-data)
        (g/transact tx-data)))))

(extend-type SourceViewer
  handler/SelectionProvider
  (selection [this] (if-let [selected-text (cvx/text-selection this)]
                      [selected-text]
                      []))
  (succeeding-selection [this] [])
  (alt-selection [this] [])

  cvx/TextContainer
  (text! [this s]
    (.set (.getDocument this) s))
  (text [this]
    (.get (.getDocument this)))
  (replace! [this offset length s]
    (try
      (-> this (.getTextWidget) (.getContent) (.replaceTextRange offset length s))
      (catch Exception e
        (println "Replace failed at offset" offset))))
  cvx/TextCaret
  (caret! [this offset select?]
    (source-viewer-set-caret! this offset select?))
  (caret [this]
    (let [c (cvx/caret (.getTextWidget this))
          doc (cvx/text this)]
      (cvx/adjust-bounds doc c)))
  cvx/TextView
  (selection-offset [this]
    (.-offset ^TextSelection (-> this (.getTextWidget) (.getSelection))))
  (selection-length [this]
    (.-length ^TextSelection (-> this (.getTextWidget) (.getSelection))))
  (text-selection [this]
    (.get (.getDocument this) (cvx/selection-offset this) (cvx/selection-length this)))
  (text-selection! [this offset length]
    (.setSelectionRange (.getTextWidget this) offset length))
  (editable? [this]
    (-> this (.getTextWidget) (.getEditable)))
  (editable! [this val]
    (-> this (.getTextWidget) (.setEditable val)))
  (screen-position [this]
    (let [tw (.getTextWidget this)
        caret-pos (cvx/caret this)
        p (.getLocationAtOffset tw caret-pos)]
      (.localToScreen tw p)))
  (text-area [this]
    (.getTextWidget this))
  (refresh! [this]
    (let [text-area (.getTextWidget this)]
      (.requestFocus text-area)
      (.requestLayout text-area)))
  (page-down [this]
    (.pageDown ^DefoldStyledTextSkin (skin this)))
  (page-up [this]
    (.pageUp ^DefoldStyledTextSkin (skin this)))
  (last-visible-row-number [this]
    (.getLastVisibleRowNumber ^DefoldStyledTextSkin (skin this)))
  (first-visible-row-number [this]
    (.getFirstVisibleRowNumber ^DefoldStyledTextSkin (skin this)))
  (show-line [this]
    (let [caret (cvx/caret this)
          skin (skin this)
          line-num (cvx/line-num-at-offset this caret)]
      (when skin (.showLine ^DefoldStyledTextSkin skin line-num))))
    cvx/TextStyles
  (styles [this] (let [document-len (-> this (.getDocument) (.getLength))
                       text-widget (.getTextWidget this)
                       len (dec (.getCharCount text-widget))
                       style-ranges (.getStyleRanges text-widget (int 0) len false)
                       style-fn (fn [sr] {:start (.-start ^StyleRange sr)
                                         :length (.-length ^StyleRange sr)
                                         :stylename (.-stylename ^StyleRange sr)})]
                   (mapv style-fn style-ranges)))
  cvx/TextUndo
  (state-changes! [this]
    (transact-changes this (g/prev-sequence-label (g/node-id->graph-id (-> this (.getTextWidget) (code-node))))))
  (typing-changes!
    ([this] (cvx/typing-changes! this false))
    ([this merge-changes]
     (when (transact-changes this (new-typing-opseq! this merge-changes))
       (restart-typing-timeout this))))
  cvx/CommandHistory
  (last-command [this] @(last-command-data this))
  (last-command! [this cmd] (reset! (last-command-data this) cmd))
  cvx/TextLine
  (line [this]
    (let [text-area-content (.getContent (.getTextWidget this))
          offset (cvx/caret this)
          line-no (.getLineAtOffset text-area-content offset)]
      (.getLine text-area-content line-no)))
  (prev-line-num [this]
   (let [text-area-content (.getContent (.getTextWidget this))
         offset (cvx/caret this)
         line-no (.getLineAtOffset text-area-content offset)]
     (dec line-no)))
  (prev-line [this]
    (let [text-area-content (.getContent (.getTextWidget this))
          prev-line-num (cvx/prev-line-num this)]
      (if (neg? prev-line-num)
        ""
        (.getLine text-area-content prev-line-num))))
  (next-line-num [this]
    (let [text-area-content (.getContent (.getTextWidget this))
          offset (cvx/caret this)
          line-no (.getLineAtOffset text-area-content offset)
          line-offset (.getOffsetAtLine text-area-content line-no)
          line-length (count (.getLine text-area-content line-no))
          doc-length (count (cvx/text this))
          end-of-doc? (<= doc-length (+ line-offset line-length))]
      (when (not end-of-doc?)
        (inc line-no))))
  (next-line [this]
    (let [text-area-content (.getContent (.getTextWidget this))
          line-num (cvx/next-line-num this)]
      (if line-num
        (.getLine text-area-content line-num)
        "")))
  (line-offset [this]
    (let [text-area-content (.getContent (.getTextWidget this))
          offset (cvx/caret this)
          line-no (.getLineAtOffset text-area-content offset)]
      (.getOffsetAtLine text-area-content line-no)))
  (line-num-at-offset [this offset]
    (let [text-area-content (.getContent (.getTextWidget this))]
      (.getLineAtOffset text-area-content offset)))
  (line-at-num [this line-num]
    (let [text-area-content (.getContent (.getTextWidget this))]
      (.getLine text-area-content line-num)))
  (line-offset-at-num [this line-num]
    (let [text-area-content (.getContent (.getTextWidget this))]
      (.getOffsetAtLine text-area-content line-num)))
  (line-count [this]
    (let [text-area-content (.getContent (.getTextWidget this))]
      (.getLineCount text-area-content)))
  cvx/TextProposals
  (propose [this]
    (when-let [assist-fn (assist this)]
      (let [offset (cvx/caret this)
            line (cvx/line this)
            line-offset (cvx/line-offset this)
            completion-text (subs line 0 (- offset line-offset))
            code-node-id (-> this (.getTextWidget) (code-node))
            completions (g/node-value code-node-id :completions)]
        (assist-fn completions (cvx/text this) offset completion-text))))
  cvx/TextOffset
  (preferred-offset [this]
    @(prefer-offset this))
  (preferred-offset! [this val]
    (reset! (prefer-offset this) val))
  cvx/TextSnippet
  (snippet-tab-triggers [this] (when-let [tt (tab-triggers this)]
                                 @tt))
  (snippet-tab-triggers! [this val] (reset! (tab-triggers this) val))
  (has-snippet-tab-trigger? [this] (:select (cvx/snippet-tab-triggers this)))
  (has-prev-snippet-tab-trigger? [this] (:prev-select (cvx/snippet-tab-triggers this)))
  (next-snippet-tab-trigger! [this]
    (let [tt (tab-triggers this)
          triggers (:select @tt)
          prev-triggers (or (:prev-select @tt) [])
          trigger (or (first triggers) :end)
          exit-trigger (:exit @tt)]
      (if (= trigger :end)
        (reset! tt nil)
        (do
          (swap! tt assoc :select (rest triggers))
          (swap! tt assoc :prev-select (cons trigger prev-triggers))))
      {:trigger trigger :exit exit-trigger}))
  (prev-snippet-tab-trigger! [this]
    (let [tt (tab-triggers this)
          triggers (:select @tt)
          prev-triggers (or (:prev-select @tt) [])
          trigger (or (second prev-triggers) :begin)
          next-trigger (first prev-triggers)
          exit-trigger (:exit @tt)]
      (when-not (= :begin trigger)
        (swap! tt assoc :select (cons next-trigger triggers))
        (swap! tt assoc :prev-select (rest prev-triggers)))
      {:trigger trigger :exit exit-trigger}))
  (clear-snippet-tab-triggers! [this]
    (reset! (tab-triggers this) nil)))

(defn- ^DefoldStyledTextSkin skin [^SourceViewer source-viewer]
  (-> source-viewer (.getTextWidget) (.getSkin)))

(defn- setup-find-bar [^GridPane find-bar source-viewer]
  (doto find-bar
    (ui/context! :find-bar {:find-bar find-bar :source-viewer source-viewer} nil)
    (.setMaxWidth Double/MAX_VALUE))
  (ui/with-controls find-bar [^CheckBox whole-word ^CheckBox case-sensitive ^CheckBox wrap ^TextField term ^Button next ^Button prev]
    (.bindBidirectional (.textProperty term) cvx/find-term)
    (.bindBidirectional (.selectedProperty whole-word) cvx/find-whole-word)
    (.bindBidirectional (.selectedProperty case-sensitive) cvx/find-case-sensitive)
    (.bindBidirectional (.selectedProperty wrap) cvx/find-wrap)
    (ui/bind-keys! find-bar {KeyCode/ENTER :find-next})
    (ui/bind-action! next :find-next)
    (ui/bind-action! prev :find-prev))
  find-bar)

(defn- setup-replace-bar [^GridPane replace-bar source-viewer]
  (doto replace-bar
    (ui/context! :replace-bar {:replace-bar replace-bar :source-viewer source-viewer} nil)
    (.setMaxWidth Double/MAX_VALUE))
  (ui/with-controls replace-bar [^CheckBox whole-word ^CheckBox case-sensitive ^CheckBox wrap ^TextField term ^TextField replacement ^Button next ^Button replace ^Button replace-all]
    (.bindBidirectional (.textProperty term) cvx/find-term)
    (.bindBidirectional (.textProperty replacement) cvx/find-replacement)
    (.bindBidirectional (.selectedProperty whole-word) cvx/find-whole-word)
    (.bindBidirectional (.selectedProperty case-sensitive) cvx/find-case-sensitive)
    (.bindBidirectional (.selectedProperty wrap) cvx/find-wrap)
    (ui/bind-action! next :find-next)
    (ui/bind-action! replace :replace-next)
    (ui/bind-keys! replace-bar {KeyCode/ENTER :replace-next})
    (ui/bind-action! replace-all :replace-all))
  replace-bar)

(defn- hide-bars [^GridPane grid]
  (let [visible-bars (filter #(= (GridPane/getRowIndex %) 1) (.. grid getChildren))]
    (.. grid getChildren (removeAll (to-array visible-bars)))))

(defn- show-bar [^GridPane grid ^Parent bar]
  (when-not (.. grid getChildren (contains bar))
    (hide-bars grid)
    (GridPane/setConstraints bar 0 1)
    (.. grid getChildren (add bar))))

(defn- focus-text-area [^SourceViewer source-viewer]
  (.requestFocus (.getTextWidget source-viewer)))

(defn- focus-term-field [^Parent bar]
  (ui/with-controls bar [^TextField term]
    (.requestFocus term)
    (.selectAll term)))

(defn- non-empty-text-selection [source-viewer]
  (let [text (cvx/text-selection source-viewer)]
    (when (seq text) text)))

(handler/defhandler :find-text :code-view
  (run [grid source-viewer find-bar]
    (when-let [text (non-empty-text-selection source-viewer)]
      (cvx/set-find-term text))
    (show-bar grid find-bar)
    (focus-term-field find-bar)))

(handler/defhandler :replace-text :code-view
  (run [grid source-viewer replace-bar]
    (when-let [text (non-empty-text-selection source-viewer)]
      (cvx/set-find-term text))
    (show-bar grid replace-bar)
    (focus-term-field replace-bar)))

(handler/defhandler :find-text :console ;; in practice, from find/replace bars
  (run [grid find-bar]
    (show-bar grid find-bar)
    (focus-term-field find-bar)))

(handler/defhandler :replace-text :console
  (run [grid replace-bar]
    (show-bar grid replace-bar)
    (focus-term-field replace-bar)))

(handler/defhandler :hide-bars :console
  (run [grid source-viewer find-bar replace-bar]
    (hide-bars grid)
    (focus-text-area source-viewer)))

(defn make-view [graph ^Parent parent code-node opts]
  (let [source-viewer (setup-source-viewer opts)
        view-id (setup-code-view (:app-view opts) (g/make-node! graph CodeView :source-viewer source-viewer) code-node (get opts :caret-position 0))
        repainter (ui/->timer 10 "refresh-code-view" (fn [_ dt] (g/node-value view-id :new-content)))
        grid (GridPane.)
        find-bar (setup-find-bar ^GridPane (ui/load-fxml "find.fxml") source-viewer)
        replace-bar (setup-replace-bar ^GridPane (ui/load-fxml "replace.fxml") source-viewer)
        context-env {:view-node view-id :clipboard (Clipboard/getSystemClipboard) :source-viewer source-viewer :grid grid :find-bar find-bar :replace-bar replace-bar}]

    (ui/context! grid :console context-env nil)
    (ui/bind-keys! grid {KeyCode/ESCAPE :hide-bars})

    (doto (.getColumnConstraints grid)
      (.add (doto (ColumnConstraints.)
              (.setHgrow Priority/ALWAYS))))

    (GridPane/setConstraints source-viewer 0 0)
    (GridPane/setVgrow source-viewer Priority/ALWAYS)

    (ui/children! grid [source-viewer])
    (ui/children! parent [grid])
    (ui/fill-control grid)
    (ui/context! source-viewer :code-view context-env source-viewer {:code-node [:view-node :resource-node]})
    (ui/observe (.selectedProperty ^Tab (:tab opts)) (fn [this old new]
                                                       (when (= true new)
                                                         (ui/run-later (cvx/refresh! source-viewer)))))
    (cvx/refresh! source-viewer)
    (ui/timer-start! repainter)
    (ui/timer-stop-on-closed! ^Tab (:tab opts) repainter)
    (g/node-value view-id :new-content)
    view-id))

(defn focus-view
  [code-view-node {:keys [line]}]
  (when-let [^SourceViewer source-viewer (g/node-value code-view-node :source-viewer)]
    (cvx/refresh! source-viewer)
    (when line
      (cvx/go-to-line source-viewer line))))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :code
                                :label "Code"
                                :make-view-fn (fn [graph ^Parent parent code-node opts] (make-view graph parent code-node opts))
                                :focus-fn focus-view))
