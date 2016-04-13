(ns editor.code-view
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
  (:import [com.defold.editor.eclipse DefoldRuleBasedScanner]
           [java.util.function Function]
           [javafx.scene Parent]
           [javafx.scene.control Control]
           [javafx.scene.image Image ImageView]
           [org.eclipse.jface.text.rules IRule SingleLineRule ICharacterScanner IPartitionTokenScanner IPredicateRule MultiLineRule WhitespaceRule IWhitespaceDetector WordRule IWordDetector RuleBasedScanner RuleBasedPartitionScanner IToken Token FastPartitioner]
           [org.eclipse.fx.text.ui.rules DefaultDamagerRepairer]
           [org.eclipse.fx.text.ui TextAttribute]
           [org.eclipse.fx.text.ui.source SourceViewer SourceViewerConfiguration]
           [org.eclipse.fx.text.ui.contentassist ICompletionProposal ContentAssistant ContentAssistContextData]
           [org.eclipse.fx.ui.controls.styledtext TextSelection]
           [org.eclipse.fx.text.ui.presentation IPresentationReconciler PresentationReconciler]
           [org.eclipse.jface.text IDocument IDocumentPartitioner Document IDocumentListener DocumentEvent]))

(set! *warn-on-reflection* true)

(defn- opseqs [text-area]
  (ui/user-data text-area ::opseqs))

(defn- new-opseq [text-area]
  (swap! (opseqs text-area) rest))

(defn- opseq [text-area]
  (first @(opseqs text-area)))

(defn- programmatic-change [text-area]
  (ui/user-data text-area ::programmatic-change))

(defn- code-node [text-area]
  (ui/user-data text-area ::code-node))

(defn- restart-opseq-timer [text-area]
  (if-let [timer (ui/user-data text-area ::opseq-timer)]
    (ui/restart timer)
    (ui/user-data! text-area ::opseq-timer (ui/->future 0.5 #(new-opseq text-area)))))

(defmacro binding-atom [a val & body]
  `(let [old-val# (deref ~a)]
     (try
       (reset! ~a ~val)
       ~@body
       (finally
         (reset! ~a old-val#)))))

(g/defnk update-source-viewer [^SourceViewer source-viewer code-node code caret-position]
  (let [text-area (.getTextWidget source-viewer)
        document (.getDocument source-viewer)]
    (ui/user-data! text-area ::code-node code-node)
    (when (not= code (.get document))
      (new-opseq text-area)
      (binding-atom (programmatic-change text-area) true
                    (try
                      (.set document code)
                      (catch Throwable e
                        (println "exception during .set!")
                        (.printStackTrace e)))))
    (when (not= caret-position (.getCaretOffset text-area))
      (new-opseq text-area)
      (binding-atom (programmatic-change text-area) true
                    (.setCaretOffset text-area caret-position)))
    [code-node code caret-position]))

(defn- make-proposal [{:keys [replacement offset length position image display-string additional-info]}]
  (reify ICompletionProposal
    (getLabel [this] display-string)
    ;;    (getReplacementOffset [this] offset)
    ;;    (getReplacementLength [this] length)
    ;;    (getCursorPosition [this] position)
    (getGraphic [this] (when image (ImageView. (Image. (io/input-stream (io/resource image))))))
    (^void apply [this ^IDocument document]
     (try
        (.replace document offset length replacement)
        (catch Exception e
          (.printStackTrace e))))
    (getSelection [this document]
      ; ?
      (TextSelection. (+ offset position) 0))))

(defn- make-proposals [hints]
  (map make-proposal hints))

(defn- make-proposal-fn [assist-fn]
  (fn [^ContentAssistContextData ctxt]
    (let [document (.document ctxt)
          offset (.offset ctxt)
          line-no (.getLineOfOffset document offset)
          line-offset (.getLineOffset document line-no)
          line (.get document line-offset (- offset line-offset))
          hints (assist-fn (.get document) offset line)]
      (make-proposals hints))))

(defn- to-function [fn]
  (reify Function
    (apply [this arg] (fn arg))))

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
      (let [result (scanner-fn (.toString scanner))]
        (if result
          (let [len (:length result)]
            (when (pos? len) (.moveForward scanner len))
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

(defn- make-reconciler [^SourceViewerConfiguration configuration ^SourceViewer source-viewer syntax]
  (let [pr (PresentationReconciler.)]
    (.setDocumentPartitioning pr (.getConfiguredDocumentPartitioning configuration source-viewer))
    (doseq [{:keys [partition rules]} syntax]
      (let [partition (get default-content-type-map partition partition)]
        (let [damager-repairer (make-multiline-dr (make-scanner rules))] ;;_(DefaultDamagerRepairer. (make-scanner rules))]
          (.setDamager pr damager-repairer partition)
          (.setRepairer pr damager-repairer partition))))
    pr))

(defn- ^SourceViewerConfiguration create-viewer-config [language]
  (let [{:keys [language syntax assist]} language]
    (proxy [SourceViewerConfiguration] []
      (getStyleclassName [] language)
      (getPresentationReconciler [source-viewer]
        (make-reconciler this source-viewer syntax))
      (getContentAssist []
        (when assist
          (ContentAssistant. (to-function (make-proposal-fn assist)))))
      (getConfiguredContentTypes [source-viewer]
        (into-array String (replace default-content-type-map (map :partition syntax)))))))

(defn- default-partition? [partition]
  (= (:type partition) :default))

(defn- make-partition-rule [rule]
  (make-rule rule (get-token (:partition rule))))

(defn- make-partition-scanner [partitions]
  (let [rules (map make-partition-rule (remove default-partition? partitions))]
    (doto (RuleBasedPartitionScanner.)
      (.setPredicateRules (into-array IPredicateRule rules)))))

(defn- ^IDocumentPartitioner make-partitioner [language]
  (when-let [partitions (:syntax language)]
    (let [legal-content-types (map :partition (remove default-partition? partitions))]
      (FastPartitioner. (make-partition-scanner partitions)
                        (into-array String legal-content-types)))))

(defn- setup-source-viewer [opts]
  (let [source-viewer (SourceViewer.)
        source-viewer-config (create-viewer-config opts)
        document (Document. "")
        partitioner (make-partitioner opts)]

    (when partitioner
      (.setDocumentPartitioner document (.getConfiguredDocumentPartitioning source-viewer-config source-viewer) partitioner)
      (.connect partitioner document))

    (.configure source-viewer source-viewer-config)
    (.setDocument source-viewer document)

    (let [text-area (.getTextWidget source-viewer)]
      (ui/user-data! text-area ::opseqs (atom (repeatedly gensym)))
      (ui/user-data! text-area ::programmatic-change (atom nil))

      (.addDocumentListener document
                            (reify IDocumentListener
                              (^void documentAboutToBeChanged [this ^DocumentEvent event])
                              (^void documentChanged [this ^DocumentEvent event]
                                (restart-opseq-timer text-area)
                                (when-not @(programmatic-change text-area)
                                  (let [new-code (.get document)]
                                    (g/transact
                                     (concat
                                      (g/operation-sequence (opseq text-area))
                                      (g/operation-label "Code Change")
                                      (g/set-property (code-node text-area) :code new-code))))))))

      (ui/observe (.caretOffsetProperty text-area)
                  (fn [_ _ new-caret-position]
                    (when-not @(programmatic-change text-area)
                      (g/transact
                       (concat
                        (g/operation-sequence (opseq text-area))
                        ;; we dont supply an operation label here, because the caret position events comes just after the text change, and it seems
                        ;; when merging transactions the latest label wins
                        (g/set-property (code-node text-area) :caret-position new-caret-position)))))))

  source-viewer))

(g/defnode CodeView
  (property source-viewer SourceViewer)
  (input code-node g/Int)
  (input code g/Str)
  (input caret-position g/Int)
  (output new-content g/Any :cached update-source-viewer))

(defn- setup-code-view [view-id code-node initial-caret-position]
  (g/transact
   (concat
    (g/connect code-node :_node-id view-id :code-node)
    (g/connect code-node :code view-id :code)
    (g/connect code-node :caret-position view-id :caret-position)
    (g/set-property code-node :caret-position initial-caret-position)))
  view-id)

(defn make-view [graph ^Parent parent code-node opts]
  (let [source-viewer (setup-source-viewer opts)
        view-id (setup-code-view (g/make-node! graph CodeView :source-viewer source-viewer) code-node (get opts :caret-position 0))]
    (ui/children! parent [source-viewer])
    (ui/fill-control source-viewer)
    (let [refresh-timer (ui/->timer 10 (fn [_] (g/node-value view-id :new-content)))
          stage (ui/parent->stage parent)]
      (ui/timer-stop-on-close! ^Tab (:tab opts) refresh-timer)
      (ui/timer-stop-on-close! stage refresh-timer)
      (ui/timer-start! refresh-timer)
      view-id)))

(defn update-caret-pos [view-id {:keys [caret-position]}]
  (when caret-position
    (when-let [code-node (g/node-value view-id :code-node)]
      (g/transact
       (concat
        (g/set-property code-node :caret-position caret-position))))))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :code
                                :label "Code"
                                :focus-fn update-caret-pos
                                :make-view-fn (fn [graph ^Parent parent code-node opts] (make-view graph parent code-node opts))))
