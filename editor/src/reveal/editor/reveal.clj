(ns editor.reveal
  (:require [cljfx.fx.button :as fx.button]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.v-box :as fx.v-box]
            [clojure.core.async :as a]
            [clojure.main :as m]
            [clojure.string :as str]
            [dynamo.graph :as g]
            editor.code.data
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [vlaaad.reveal :as r]
            [internal.system :as is])
  (:import [clojure.core.async.impl.channels ManyToManyChannel]
           [clojure.lang IRef]
           [editor.resource FileResource ZipResource]
           [editor.code.data Cursor CursorRange]
           [internal.graph.types Endpoint]
           [javafx.scene Parent]))

(defn- node-value-or-err [ec node-id label]
  (try
    [(g/node-value node-id label ec) nil]
    (catch Exception e
      [nil e])))

(defn- node-value-or-err->sf [[v e]]
  (if e
    (let [{:clojure.error/keys [cause class]} (-> e Throwable->map m/ex-triage)]
      (r/as e (r/raw-string (or cause class) {:fill :error})))
    (r/stream v)))

(defn- node-id-sf [{:keys [basis] :as ec} node-id]
  (let [type-sym (symbol (:k (g/node-type* basis node-id)))]
    (r/raw-string
      (str type-sym
           (when (g/node-instance? basis resource-node/ResourceNode node-id)
             (str "@" (or (resource/proj-path (g/node-value node-id :resource ec))
                          "[in-memory]")))
           "#" node-id)
      {:fill :scalar})))

(declare label-tree-node)

(defn- node-children-fn [ec node-id]
  (fn []
    (let [node-type-def @(g/node-type* (:basis ec) node-id)]
      (->> [:input :property :output]
           (mapcat (fn [k] (keys (get node-type-def k))))
           distinct
           sort
           (map
             (fn [label]
               (label-tree-node ec node-id label)))))))

(defn- label-tree-node [{:keys [basis] :as ec} node-id label]
  (let [[v e :as v-or-e] (node-value-or-err ec node-id label)
        sources (g/sources-of basis node-id label)
        targets (g/targets-of basis node-id label)]
    (cond->
      {:value (or e v)
       :annotation {::node-id+label [node-id label]}
       :render (r/horizontal
                 (r/stream label)
                 r/separator
                 (node-value-or-err->sf v-or-e))}
      (or (seq sources) (seq targets))
      (assoc :children #(map (fn [[relation [rel-node-id related-label]]]
                               (let [[v e :as v-or-e] (node-value-or-err ec rel-node-id related-label)]
                                 {:value (or e v)
                                  :render (r/horizontal
                                            (r/raw-string
                                              ({:source "<- " :target "-> "} relation)
                                              {:fill :util})
                                            (r/stream related-label)
                                            (r/raw-string " of " {:fill :util})
                                            (node-id-sf ec rel-node-id)
                                            (r/raw-string ": " {:fill :util})
                                            (node-value-or-err->sf v-or-e))
                                  :children (node-children-fn ec rel-node-id)}))
                             (concat (map (fn [x] [:source x]) sources)
                                     (map (fn [x] [:target x]) targets)))))))

(defn root-tree-node [ec node-id]
  {:value node-id
   :render (node-id-sf ec node-id)
   :children (node-children-fn ec node-id)})

(r/defaction ::defold:node-tree [x ann]
  (when (g/node-id? x)
    (let [ec (or (::evaluation-context ann) (g/make-evaluation-context))]
      (when (g/node-by-id (:basis ec) x)
        (fn []
          {:fx/type r/tree-view
           :branch? :children
           :render #(:render % (:value %))
           :valuate :value
           :annotate :annotation
           :children #((:children %))
           :root (root-tree-node ec x)})))))

(defn node-id-in-context
  ([node-id]
   (g/with-auto-evaluation-context evaluation-context
     (node-id-in-context node-id evaluation-context)))
  ([node-id evaluation-context]
   (r/stream node-id {::evaluation-context evaluation-context})))

(defn- de-duplicating-observable [ref f]
  (reify IRef
    (deref [_] (f @ref))
    (addWatch [this key callback]
      (add-watch ref [this key] (fn [_ _ old new]
                                  (let [old (f old)
                                        new (f new)]
                                    (when-not (= old new)
                                      (callback key this old new))))))
    (removeWatch [this key]
      (remove-watch ref [this key]))))

(defn watch-all [node-id label]
  {:fx/type r/ref-watch-all-view
   :ref (de-duplicating-observable
          g/*the-system*
          (fn [sys]
            (g/node-value node-id label (is/default-evaluation-context sys))))})

(r/defaction ::defold:watch [_ {::keys [node-id+label]}]
  (when node-id+label
    #(apply watch-all node-id+label)))

(r/defstream Endpoint [endpoint]
  (r/horizontal
    (r/raw-string "#g/endpoint [" {:fill :object})
    (r/stream (g/endpoint-node-id endpoint))
    r/separator
    (r/stream (g/endpoint-label endpoint))
    (r/raw-string "]" {:fill :object})))

(r/defstream FileResource [resource]
  (r/horizontal
    (r/raw-string "#resource/file" {:fill :object})
    r/separator
    (r/stream (resource/proj-path resource))))

(r/defstream ZipResource [resource]
  (r/horizontal
    (r/raw-string "#resource/zip" {:fill :object})
    r/separator
    (r/stream (resource/proj-path resource))))

(r/defstream Cursor [{:keys [row col]}]
  (r/horizontal
    (r/raw-string "#code/cursor [" {:fill :object})
    (r/stream row)
    r/separator
    (r/stream col)
    (r/raw-string "]" {:fill :object})))

(r/defstream CursorRange [{:keys [from to] :as range}]
  (r/horizontal
    (r/raw-string "#code/range [" {:fill :object})
    (apply
      r/vertical
      (r/horizontal
        (r/stream ((juxt :row :col) from))
        r/separator
        (r/stream ((juxt :row :col) to)))
      (let [rest (dissoc range :from :to)]
        (when (seq rest)
          [(r/vertically (map r/horizontally rest))])))
    (r/raw-string "]" {:fill :object})))

(r/defaction ::javafx:children [x]
  (when (instance? Parent x)
    (constantly {:fx/type r/tree-view
                 :render (fn [^Parent node]
                           (r/horizontal
                             (r/raw-string (.getName (class node)) {:fill :object})
                             (r/raw-string (format "@%08x" (System/identityHashCode node))
                                           {:fill :util})
                             (r/raw-string (str (when-let [id (.getId node)]
                                                  (str "#" id))
                                                (when-let [style-classes (seq (.getStyleClass node))]
                                                  (str/join (map #(str "." %) style-classes)))
                                                (when-let [pseudo-classes (seq (.getPseudoClassStates node))]
                                                  (str/join (map #(str ":" %) pseudo-classes))))
                                           {:fill :string})))
                 :root x
                 :branch? #(and (instance? Parent %)
                                (seq (.getChildrenUnmodifiable ^Parent %)))
                 :children #(vec (.getChildrenUnmodifiable ^Parent %))})))

(r/defstream ManyToManyChannel [ch]
  (r/horizontal
    (r/raw-string "(a/chan " {:fill :object})
    (r/raw-string (format "#_0x%08x" (System/identityHashCode ch)) {:fill :util})
    (r/raw-string ")" {:fill :object})))

(r/defaction ::lsp:watch [x]
  (when (= :editor.lsp/running-lsps (:type (meta x)))
    (fn []
      {:fx/type r/observable-view
       :ref x
       :fn (fn [in->state]
             {:fx/type fx.v-box/lifecycle
              :children (for [[in state] in->state]
                          {:fx/type fx.v-box/lifecycle
                           :v-box/vgrow :always
                           :children [{:fx/type fx.h-box/lifecycle
                                       :alignment :center-left
                                       :padding 10
                                       :spacing 5
                                       :children [{:fx/type fx.label/lifecycle
                                                   :max-width ##Inf
                                                   :h-box/hgrow :always
                                                   :text (format "LSP server #0x%08x" (System/identityHashCode in))}
                                                  {:fx/type fx.button/lifecycle
                                                   :focus-traversable false
                                                   :text "Shutdown"
                                                   :on-action (fn [_] (a/close! in))}]}
                                      {:fx/type r/value-view
                                       :v-box/vgrow :always
                                       :value state}]})})})))