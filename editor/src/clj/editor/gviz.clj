;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.gviz
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.fs :as fs]
            [editor.ui :as ui]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [util.fn :as fn])
  (:import [java.io BufferedWriter File IOException StringWriter]
           [javafx.scene.paint Color]))

(set! *warn-on-reflection* true)

(defn installed? []
  (try
    (let [p (.exec (Runtime/getRuntime) "dot -V")]
      (.waitFor p)
      (= 0 (.exitValue p)))
    (catch IOException e
      false)))

(defn- source [[source _ _ _]] source)
(defn- target [[_ _ target _]] target)

(defn- gen-file [ext] (fs/create-temp-file! "graph" ext))

(defonce ^:private ^:dynamic ^File *dot-file* (gen-file ".dot"))
(defonce ^:private ^:dynamic ^File *png-file* (gen-file ".png"))

(defn- writer ^BufferedWriter []
  (BufferedWriter. (try
                     (io/writer *dot-file*)
                     (catch Throwable t
                       (alter-var-root #'*dot-file* (fn [_] (gen-file ".png")))
                       (io/writer *dot-file*)))))

(defn- write [^BufferedWriter w ^String s]
  (.write w s)
  (.newLine w))

(defn- node-type-name [basis node-id]
  (when-let [t (g/node-type* basis node-id)]
    (:name @t)))

(defn- flatten-arcs [arcs]
  (mapcat (fn [[_node-id label->arc-table]]
            (mapcat (comp ig/arc-table-arcs val) label->arc-table))
          arcs))

(defn- nodes->arcs [basis nodes]
  (let [nodes (set nodes)]
    (into #{}
          (comp
            (filter (fn [arc]
                      (or (contains? nodes (:source-id arc))
                          (contains? nodes (:target-id arc)))))
            (map (fn [arc]
                   [(:source-id arc) (:source-label arc) (:target-id arc) (:target-label arc)])))
          (concat (flatten-arcs (gt/sarcs basis))
                  (flatten-arcs (gt/tarcs basis))))))

(defn escape-field-label [label]
  (-> label
      (string/replace " " "\\ ")
      (string/replace "<" "\\<")
      (string/replace ">" "\\>")
      (string/replace "|" "\\|")))

(defn write-node [w basis node-id color inputs outputs]
  (let [inputs (map name (set (inputs node-id)))
        outputs (map name (set (outputs node-id)))
        node-label (node-type-name basis node-id)
        color (if node-label color "red")
        node-label (or node-label "Unknown")
        label (let [input-fields (string/join "|" (map (fn [x] (format "<_in-%s>%s" (escape-field-label x) (escape-field-label x))) inputs))
                    input-fields (when-not (string/blank? input-fields)
                                   (str "{" input-fields "}"))
                    output-fields (string/join "|" (map (fn [x] (format "<_out-%s>%s" (escape-field-label x) (escape-field-label x))) outputs))
                    output-fields (when-not (string/blank? output-fields)
                                    (str "{" output-fields "}"))
                    fields (string/join "|" (remove string/blank? [input-fields output-fields]))
                    fields (when-not (string/blank? fields)
                             (str "{" fields "}"))]
                (str node-label "|<_nid>" node-id (when-not (string/blank? fields) "|") fields))]
    (write w (format "%s [shape=record, label=\"%s\", color=\"%s\", fontcolor=\"%s\"];" node-id label color color))))

(defn- include-overrides [basis nodes]
  (let [child-fn (partial g/overrides basis)]
    (mapcat #(tree-seq fn/constantly-true child-fn %) nodes)))

(defn- extract-nodes [basis opts]
  (->> (if (:root-id opts)
         (let [root-id (:root-id opts)
               input-fn (:input-fn opts fn/constantly-false)
               output-fn (:output-fn opts fn/constantly-false)]
           (mapcat
             (fn [[f arcs-fn key]]
               (g/pre-traverse basis [root-id] (fn [basis node-id]
                                                 (map key (filter f (arcs-fn basis node-id))))))
             [[input-fn g/inputs gt/source-id]
              [output-fn g/outputs gt/target-id]]))
         (keys (gt/nodes basis)))
       (include-overrides basis)))

(defn subgraph->dot ^String [basis & {:keys [root-id input-fn output-fn] :or {root-id nil} :as opts}]
  (let [nodes (extract-nodes basis opts)
        arcs (nodes->arcs basis nodes) inputs (reduce (fn [inputs [s sl t tl]] (update inputs t conj tl)) {} arcs)
        outputs (reduce (fn [outputs [s sl t tl]] (update outputs s conj sl)) {} arcs)
        sw (StringWriter.)
        node-set (set nodes)
        all-nodes (set (keys (gt/nodes basis)))
        referred-nodes (mapcat (fn [[s _ t _]] [s t]) arcs)
        excluded-nodes (set (filter (complement node-set) referred-nodes))]
    (with-open [w (BufferedWriter. sw)]
      (write w "digraph G {")
      (write w "rankdir=LR")

      (doseq [node-id (concat nodes excluded-nodes)
              :let [color (if (excluded-nodes node-id) "grey" "black")]]
        (write-node w basis node-id color inputs outputs))

      (doseq [[source source-label target target-label] arcs]
        (let [color (if (or (not (all-nodes source)) (not (all-nodes target)))
                      "red"
                      (if (or (excluded-nodes source) (excluded-nodes target))
                        "grey"
                        "black"))]
          (write w (format "%s:\"_out-%s\" -> %s:\"_in-%s\" [color=\"%s\"];" source (name source-label) target (name target-label) color))))
      (doseq [node-id nodes]
        (when-let [original (g/override-original basis node-id)]
          (write w (format "%s:\"_nid\" -> %s:\"_nid\" [style=dashed];" node-id original))))
      (write w "}"))
    (.toString sw)))

(defn dot->image ^File [^String dot]
  (with-open [w (writer)]
    (.write w dot))
  (let [p (.exec (Runtime/getRuntime) (format "dot %s -Tpng -o%s" *dot-file* *png-file*))]
    (.waitFor p)
    (when (= 0 (.exitValue p))
      *png-file*)))

(defn show [^String dot]
  (if-let [image (dot->image dot)]
    (ui/open-file image)
    :fail))

(defn show-graph [basis & {:keys [root-id input-fn output-fn] :or {root-id nil} :as opts}]
  (let [f (-> (apply subgraph->dot basis (mapcat identity opts))
            (dot->image))]
    (when f
      (ui/open-file f))))

(defn show-internal-node-type-successors
  "Show the graph of internal connections for a particular node type

  This graph does not show connections from every property to
  _declared-properties intrinsic because it creates too much noise

  Color codes:
    blue - inputs
    green - properties
    beige - outputs
    purple - intrinsics"
  [node-type]
  (let [label->graphviz-id #(Compiler/munge (name %))
        type-value @node-type
        deps (into {} (map (juxt key (comp :dependencies val)) (:output type-value)))
        all-labels (into (set (keys deps)) cat (vals deps))
        dot (str "digraph G {
                    edge [ arrowsize = 0.5, color=\"#666666\" ]
                    pack=10
                    fontsize=64
                    labelloc=t
                    label=\"Internal successors of " (name (:k node-type)) "\"
                    rankdir=LR
                    splines=true
                    overlap=false
                 "
                 (->> all-labels
                      (map (fn [label]
                             (str (label->graphviz-id label) "[label=\"" (name label)
                                  "\", style=\"filled,rounded\",peripheries=0, shape=box, fillcolor=\""
                                  (if (#{:_declared-properties :_this :_node-id} label)
                                    "#e4cbef"
                                    (condp get label
                                      (:property type-value) "#b1edbc"
                                      (:input type-value) "#b8e5f9"
                                      (:output type-value) "#e8e1b9"
                                      "#ededed"))
                                  "\"]")))
                      (string/join "\n"))
                 "\n"
                 (string/join
                   "\n" (for [[to from-labels] deps
                              from from-labels
                              :when (and (not= from :_node-id)
                                         (not= to :_declared-properties))]
                          (str (label->graphviz-id from) " -> " (label->graphviz-id to) " []")))
                 "\n}")]
    (show dot)))

(defn interpolate-colors [gradient value]
  (let [value (-> value
                  (max (key (first (seq gradient))))
                  (min (key (first (rseq gradient)))))
        [from-value from-color] (first (rsubseq gradient <= value))
        [to-value to-color] (first (subseq gradient >= value))
        offset (- to-value from-value)
        interpolation (double (if (zero? offset)
                                0.0
                                (/ (- value from-value)
                                   offset)))]
    (str "#" (-> (Color/valueOf ^String from-color)
                 (.interpolate (Color/valueOf ^String to-color) interpolation)
                 (str)
                 (subs 2 8)))))

(defn show-external-node-type-connections-between-nodes [node-ids]
  (let [basis (g/now)
        intermediates (into {}
                            (map (juxt identity
                                       (fn [node-id]
                                         (into (g/inputs basis node-id) (g/outputs basis node-id)))))
                            node-ids)
        edge-frequencies-doubled (frequencies
                                   (eduction
                                     cat
                                     (filter (fn [arc]
                                               (and (intermediates (gt/source-id arc))
                                                    (intermediates (gt/target-id arc)))))
                                     (remove (fn [arc]
                                               (and (= (gt/source-label arc) :_node-id)
                                                    (= (gt/target-label arc) :nodes))))
                                     (map (fn [arc]
                                            [(name (:k (g/node-type* basis (gt/source-id arc))))
                                             (gt/source-label arc)
                                             (name (:k (g/node-type* basis (gt/target-id arc))))
                                             (gt/target-label arc)]))
                                     (vals intermediates)))
        edges (keys edge-frequencies-doubled)
        nodes (reduce
                (fn [acc [from from-label to to-label]]
                  (-> acc
                      (update-in [from :out] (fnil conj #{}) from-label)
                      (update-in [to :in] (fnil conj #{}) to-label)))
                {}
                edges)]
    (show
      (str "digraph G {
                ranksep=2
                rankdir=LR
                overlap=false
                splines=true
                node [shape=none]
                edge [ arrowsize = 0.5, color=\"#666666\" ]
             "
           (->> nodes
                (map (fn [[type-name {:keys [in out]}]]
                       (let [rows (max (count in) (count out))
                             in (vec (sort in))
                             out (vec (sort out))
                             escape-html-string #(-> %
                                                     (string/replace "&" "&amp;")
                                                     (string/replace "<" "&lt;")
                                                     (string/replace ">" "&gt;")
                                                     (string/replace "\"" "&quot;"))]
                         (str type-name
                              " [label=<<table color=\"#cccccc\" border=\"0\" cellborder=\"1\" cellspacing=\"0\" bgcolor=\"#ededed\">
                                             <tr>
                                               <td colspan=\"2\"><b>" type-name "</b></td>
                                            </tr>
                                "
                              (string/join
                                "\n"
                                (for [i (range rows)]
                                  (format "<tr>
                                                   <td port=\"%s\">%s</td>
                                                   <td port=\"%s\">%s</td>
                                                 </tr>"
                                          (Compiler/munge (str "in" (name (get in i ""))))
                                          (escape-html-string (name (get in i "")))
                                          (Compiler/munge (str "out" (name (get out i ""))))
                                          (escape-html-string (name (get out i ""))))))
                              "        </table>>]\n"))))
                (string/join "\n"))
           "\n"
           (->> edges
                (map (fn [[from from-label to to-label :as edge]]
                       (let [weight (Math/log10 (/ (edge-frequencies-doubled edge) 2))]
                         (str from ":out" (Compiler/munge (name from-label))
                              " -> "
                              to ":in" (Compiler/munge (name to-label))
                              "[color=\"" (interpolate-colors (sorted-map 0.0 "#94d2bd"  ;; 1
                                                                          3.0 "#ee9b00"  ;; 1000
                                                                          6.0 "#ae2012") ;; 1000000
                                                              weight) "\", weight=" weight"]"))))
                (string/join "\n"))
           "}"))))

(defn show-external-node-type-connections-for-scoped-node [node-id]
  (show-external-node-type-connections-between-nodes
    (conj (g/node-value node-id :nodes) node-id)))

(defn show-external-node-type-connections-for-all-scoped-nodes-of-type [node-type]
  (show-external-node-type-connections-between-nodes
    (->> (gt/nodes (g/now))
         (filter (comp #{node-type} :node-type val))
         (map key)
         (mapcat #(conj (g/node-value % :nodes) %)))))

(defn show-external-node-type-connections-for-all-nodes-of-ns-type [node-type-ns]
  (show-external-node-type-connections-between-nodes
    (->> (gt/nodes (g/now))
         (filter (comp #{node-type-ns} namespace :k :node-type val))
         (map key))))

(comment
  (show-internal-node-type-successors editor.gui/GuiSceneNode)
  ;; selected scoped node
  (show-external-node-type-connections-for-scoped-node (first (dev/selection)))
  ;; all project nodes
  (show-external-node-type-connections-between-nodes (keys (gt/nodes (g/now))))
  ;; particular type of scope
  (show-external-node-type-connections-for-all-scoped-nodes-of-type editor.gui/GuiSceneNode)
  (show-external-node-type-connections-for-all-nodes-of-ns-type "editor.gui"))
