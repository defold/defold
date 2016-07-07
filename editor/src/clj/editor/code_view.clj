(ns editor.code-view
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.code-view-ux :as cvx]
            [editor.core :as core]
            [editor.handler :as handler]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
  (:import [com.defold.editor.eclipse DefoldRuleBasedScanner Document DefoldStyledTextSkin DefoldStyledTextSkin$LineCell
            DefoldStyledTextBehavior DefoldStyledTextArea DefoldSourceViewer DefoldStyledTextLayoutContainer]
           [javafx.scene Parent]
           [javafx.scene.input Clipboard ClipboardContent KeyEvent MouseEvent]
           [javafx.scene.image Image ImageView]
           [java.util.function Function]
           [javafx.scene.control ListView ListCell Tab]
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

(ui/extend-menu ::text-edit :editor.app-view/edit
                (cvx/create-menu-data))

(defn- code-node [text-area]
  (ui/user-data text-area ::code-node))

(defn- behavior [text-area]
  (ui/user-data text-area ::behavior))

(defn- assist [selection]
  (ui/user-data selection ::assist))

(defn- syntax [selection]
  (ui/user-data selection ::syntax))

(defn- prefer-offset [selection]
  (ui/user-data selection ::prefer-offset))

(defn- tab-triggers [selection]
  (ui/user-data selection ::tab-triggers))

(defmacro binding-atom [a val & body]
  `(let [old-val# (deref ~a)]
     (try
       (reset! ~a ~val)
       ~@body
       (finally
         (reset! ~a old-val#)))))

(g/defnk update-source-viewer [^SourceViewer source-viewer code-node code caret-position selection-offset selection-length active-tab prefer-offset tab-triggers]
  (ui/user-data! (.getTextWidget source-viewer) ::code-node code-node)
    (when (not= code (cvx/text source-viewer))
    (try
      (cvx/text! source-viewer code)
      (catch Throwable e
        (println "exception during .set!")
        (.printStackTrace e))))
    (cvx/preferred-offset! source-viewer prefer-offset)
    (cvx/snippet-tab-triggers! source-viewer tab-triggers)
  (if (pos? selection-length)
    (do
      ;; There is a bug somewhere in the e(fx)clipse that doesn't
      ;; display the text selection property after you change the text programatically
      ;; when it's resolved uncomment
      (cvx/caret! source-viewer caret-position false)
      (cvx/text-selection! source-viewer selection-offset selection-length)
    )
    (cvx/caret! source-viewer caret-position false))
  [code-node code caret-position])


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
            result (scanner-fn (.readString sc))]
        (if result
          (let [len (:length result)]
            (when (pos? len) (.moveForward sc len))
            token)
          Token/UNDEFINED)))
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
              :line-length (.getLineLength de)})

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

(defn setup-source-viewer [opts use-custom-skin?]
  (let [source-viewer (DefoldSourceViewer.)
        source-viewer-config (create-viewer-config source-viewer opts)
        document (Document. "")
        partitioner (make-partitioner opts)]

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
                                  (defoldUpdateCursor [^MouseEvent event visible-cells selection]
                                    (try
                                      (let [vcells (into [] visible-cells)
                                           doc-len (.getCharCount ^StyledTextArea text-area)
                                           event-y (.getY event)
                                           scene-event-x (.getSceneX event)
                                           scene-event-y (.getSceneY event)
                                           caret-cells (sort-by :min-y (mapv #(caret-at-point % scene-event-x scene-event-y) vcells))
                                           found-cells (filter #(when-let [idx (:caret-idx %)]
                                                                  (not= -1 idx)) caret-cells)]
                                       (if-let [fcell (first found-cells)]
                                         (cvx/caret! text-area (+ (:start-offset fcell) (:caret-idx fcell)) selection)
                                         (let [closest-cell (some #(when (>= (:max-y %) event-y) %) caret-cells)]
                                           (cond

                                             (:line-offset closest-cell)
                                             (cvx/caret! text-area (+ (:line-offset closest-cell) (:line-length closest-cell)) selection)

                                             true
                                             (cvx/caret! text-area doc-len selection)))))
                                      (catch Exception e (println "error updating cursor")))))]
      (.addEventHandler ^StyledTextArea text-area
                        KeyEvent/KEY_PRESSED
                        (ui/event-handler e (cvx/handle-key-pressed e source-viewer)))
      (.addEventHandler ^StyledTextArea text-area
                        KeyEvent/KEY_TYPED
                        (ui/event-handler e (cvx/handle-key-typed e source-viewer)))
     (when use-custom-skin?
       (let [skin (new DefoldStyledTextSkin text-area styled-text-behavior)]
         (.setSkin text-area skin)
         (.addEventHandler  ^ListView (.getListView skin)
                            MouseEvent/MOUSE_CLICKED
                            (ui/event-handler e (cvx/handle-mouse-clicked e source-viewer)))))


      (ui/user-data! text-area ::behavior styled-text-behavior)
      (ui/user-data! source-viewer ::assist (:assist opts))
      (ui/user-data! source-viewer ::syntax (:syntax opts))
      (ui/user-data! source-viewer ::prefer-offset (atom 0))
      (ui/user-data! source-viewer ::tab-triggers (atom nil)))

  source-viewer))

(g/defnode CodeView
  (property source-viewer SourceViewer)
  (input code-node g/Int)
  (input code g/Str)
  (input caret-position g/Int)
  (input prefer-offset g/Int)
  (input tab-triggers g/Any)
  (input selection-offset g/Int)
  (input selection-length g/Int)
  (input active-tab Tab)
  (output new-tab-active g/Any :cached  (g/fnk [source-viewer active-tab]
                                                  (cvx/refresh! source-viewer)
                                                active-tab))
  (output new-content g/Any :cached update-source-viewer))

(defn setup-code-view [app-view-id view-id code-node initial-caret-position]
  (g/transact
   (concat
    (g/connect code-node :_node-id view-id :code-node)
    (g/connect code-node :code view-id :code)
    (g/connect code-node :caret-position view-id :caret-position)
    (g/connect code-node :prefer-offset view-id :prefer-offset)
    (g/connect code-node :tab-triggers view-id :tab-triggers)
    (g/connect code-node :selection-offset view-id :selection-offset)
    (g/connect code-node :selection-length view-id :selection-length)
    (g/connect app-view-id :active-tab view-id :active-tab)
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

(defn source-viewer-set-caret! [source-viewer offset select?]
  (cvx/caret! (.getTextWidget ^SourceViewer source-viewer) offset select?))

(extend-type SourceViewer
  workspace/SelectionProvider
  (selection [this] this)
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
  (changes! [this]
    (let [code-node-id (-> this (.getTextWidget) (code-node))
          selection-offset (cvx/selection-offset this)
          selection-length (cvx/selection-length this)
          code (cvx/text this)
          caret (cvx/caret this)
          prefer-offset (cvx/preferred-offset this)
          tab-triggers (cvx/snippet-tab-triggers this)
          code-changed? (not= code (g/node-value code-node-id :code))
          caret-changed? (not= caret (g/node-value code-node-id :caret-position))
          selection-changed? (or (not= selection-offset (g/node-value code-node-id :selection-offset))
                                 (not= selection-length (g/node-value code-node-id :selection-length)))
          prefer-offset-changed? (not= prefer-offset (g/node-value code-node-id :prefer-offset))
          tab-triggers-changed? (not= tab-triggers (g/node-value code-node-id :tab-triggers))]
      (when (or code-changed? caret-changed? selection-changed? prefer-offset-changed? tab-triggers-changed?)
        (g/transact (remove nil?
                            (concat
                             (when code-changed?  [(g/set-property code-node-id :code code)])
                             (when caret-changed? [(g/set-property code-node-id :caret-position caret)])
                             (when selection-changed?
                               [(g/set-property code-node-id :selection-offset selection-offset)
                                (g/set-property code-node-id :selection-length selection-length)])
                             (when prefer-offset-changed? [(g/set-property code-node-id :prefer-offset prefer-offset)])
                             (when tab-triggers-changed? [(g/set-property code-node-id :tab-triggers tab-triggers)])))))))
  cvx/TextLine
  (line [this]
    (let [text-area-content (.getContent (.getTextWidget this))
          offset (cvx/caret this)
          line-no (.getLineAtOffset text-area-content offset)]
      (.getLine text-area-content line-no)))
  (prev-line [this]
    (let [text-area-content (.getContent (.getTextWidget this))
          offset (cvx/caret this)
          line-no (.getLineAtOffset text-area-content offset)
          prev-line-num (dec line-no)]
      (if (neg? prev-line-num)
        ""
        (.getLine text-area-content prev-line-num))))
  (next-line [this]
    (let [text-area-content (.getContent (.getTextWidget this))
          offset (cvx/caret this)
          line-no (.getLineAtOffset text-area-content offset)
          line-offset (.getOffsetAtLine text-area-content line-no)
          line-length (count (.getLine text-area-content line-no))
          doc-length (count(cvx/text this))
          end-of-doc? (<= doc-length (+ line-offset line-length))]
      (if end-of-doc?
        ""
        (.getLine text-area-content (inc line-no)))))
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
            code-node-id (-> this (.getTextWidget) (code-node))
            completions (g/node-value code-node-id :completions)]
        (assist-fn completions (cvx/text this) offset line))))
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
  (next-snippet-tab-trigger! [this]
    (let [tt (tab-triggers this)
          triggers (:select @tt)
          trigger (or (first triggers) :end)
          exit-trigger (:exit @tt)]
      (if (= trigger :end)
        (reset! tt nil)
        (swap! tt assoc :select (rest triggers)))
      {:trigger trigger :exit exit-trigger}))
  (clear-snippet-tab-triggers! [this]
    (reset! (tab-triggers this) nil)))

(defn make-view [graph ^Parent parent code-node opts]
  (let [source-viewer (setup-source-viewer opts true)
        view-id (setup-code-view (:app-view opts) (g/make-node! graph CodeView :source-viewer source-viewer) code-node (get opts :caret-position 0))]
    (ui/children! parent [source-viewer])
    (ui/fill-control source-viewer)
    (ui/context! source-viewer :code-view {:code-node code-node :view-node view-id :clipboard (Clipboard/getSystemClipboard)} source-viewer)
    (g/node-value view-id :new-content)
    (g/node-value view-id :new-tab-active)
    (let [refresh-timer (ui/->timer 1 "collect-text-editor-changes" (fn [_]
                                                                      (g/node-value view-id :new-tab-active)
                                                                      (cvx/changes! source-viewer)))
          stage (ui/parent->stage parent)]
      (ui/timer-stop-on-close! ^Tab (:tab opts) refresh-timer)
      (ui/timer-stop-on-close! stage refresh-timer)
      (ui/timer-start! refresh-timer))
    view-id))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :code
                                :label "Code"
                                :make-view-fn (fn [graph ^Parent parent code-node opts] (make-view graph parent code-node opts))))
