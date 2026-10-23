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

(ns util.long-map-test
  "Shared public-API generated contracts for LongMap."
  (:require [clojure.test :refer [deftest is]]
            [clojure.test.check :as tc]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [util.coll :as coll])
  (:import [clojure.lang IFn]
           [com.defold.util LongMap LongMap$Transient]
           [java.io ByteArrayInputStream ByteArrayOutputStream ObjectInputStream ObjectOutputStream ObjectStreamClass]
           [java.util Map]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private long-map
  {:label :long-map
   :initial LongMap/EMPTY
   :update (fn update-long-map [table ^long id ^IFn function present-only]
             (if (instance? LongMap table)
               (if present-only
                 (.updateIfPresent ^LongMap table id function)
                 (.update ^LongMap table id function))
               (if present-only
                 (.updateIfPresent ^LongMap$Transient table id function)
                 (.update ^LongMap$Transient table id function))))})

(def ^:private key-gen
  (gen/frequency [[5 (gen/choose -128 128)] [3 gen/large-integer]
                  [2 (gen/elements [Long/MIN_VALUE Long/MAX_VALUE 4294967296 8589934592])]]))
(def ^:private edit-gen (gen/tuple gen/boolean key-gen gen/any-printable-equatable))

(defn- edit-one [table [remove-key id value]]
  (if remove-key (dissoc table id) (assoc table id value)))

(defn- edit-many [table edits editable]
  (if editable
    (persistent! (reduce (fn [editor [remove-key id value]]
                           (if remove-key (dissoc! editor id) (assoc! editor id value)))
                         (transient table) edits))
    (reduce edit-one table edits)))

(defn- equivalent? [actual expected]
  (and (= actual expected) (= expected actual)
       (= (count actual) (count expected))
       (= (hash actual) (hash expected))
       (= (vec actual) (vec (into (sorted-map) expected)))
       (= (vec (rseq actual)) (vec (rseq (into (sorted-map) expected))))
       (= expected (reduce-kv assoc {} actual))
       (= (vec actual) (reduce conj [] actual))
       (= (meta actual) (meta expected))))

;; Model-based generated historical branching and editable batches exercise
;; both mutation implementations without relying on their internal layout.
(defn- validate-variant! [{:keys [label initial]}]
  (let [result
        (tc/quick-check
          200
          (prop/for-all [steps (gen/scale #(min 35 ^long %)
                                          (gen/vector (gen/tuple gen/nat (gen/vector edit-gen) gen/boolean)))]
            (let [metadata {:prototype label}
                  versions (reduce
                             (fn [versions [^long source edits editable]]
                               (let [[actual expected] (nth versions (mod source (count versions)))]
                                 (conj versions [(edit-many actual edits editable)
                                                 (reduce edit-one expected edits)])))
                             [[(with-meta initial metadata) (with-meta {} metadata)]]
                             steps)]
              (coll/every? (fn [[actual expected]] (equivalent? actual expected)) versions))))]
    (println :property label (dissoc result :result-data))
    (flush)
    (assert (:pass? result))
    (doseq [^long base [Long/MIN_VALUE -4096 0 4294967296 (- Long/MAX_VALUE 4095)]
            size [16 32 64 512 4096]]
      (let [entries (mapv (fn [^long index] [(unchecked-add base index) index]) (range size))
            original (into initial entries)
            expected (into {} entries)
            changed (persistent! (reduce (fn [editor [id _]] (dissoc! editor id))
                                         (transient original) (subvec entries 0 (quot (count entries) 2))))]
        (assert (equivalent? original expected))
        (assert (equivalent? changed (reduce dissoc expected (mapv first (subvec entries 0 (quot (count entries) 2))))))))))

;; Generated full-width keys force sparse leaf splits and subsequent mutations.
(defn- validate-sparse! [{:keys [label initial]}]
  (let [result
        (tc/quick-check
          60
          (prop/for-all [ids (gen/vector (gen/large-integer* {:min Long/MIN_VALUE :max Long/MAX_VALUE}) 4096)
                         size (gen/elements [64 128 129 512 4096])
                         values (gen/scale #(min 15 ^long %) (gen/vector gen/any-printable-equatable 1 16))
                         edits (gen/vector (gen/tuple gen/boolean (gen/choose 0 4095) gen/any-printable-equatable))]
            (let [entries (mapv (fn [^long index] [(nth ids index) (nth values (rem index (count values)))]) (range size))
                  expected (into {} entries)
                  original (into initial entries)
                  addressed (mapv (fn [[remove-key ^long index value]] [remove-key (nth ids index) value]) edits)
                  versions (reduce (fn [versions edit]
                                     (let [[actual expected] (peek versions)]
                                       (conj versions [(edit-one actual edit) (edit-one expected edit)])))
                                   [[original expected]] addressed)
                  editable-result (edit-many original addressed true)]
              (and (equivalent? original expected)
                   (equivalent? editable-result (second (peek versions)))
                   (coll/every? (fn [[actual expected]] (equivalent? actual expected)) versions)))))]
    (println :sparse-property label (select-keys result [:pass? :num-tests :seed :time-elapsed-ms]))
    (flush)
    (assert (:pass? result))))

;; Generated accumulators verify boxed/unboxed primitive reductions, empty-init
;; identity and early termination. Guards against narrowing empty accumulators.
(defn- validate-numeric! [{:keys [label initial]}]
  (let [result (tc/quick-check
                 200
                 (prop/for-all [expected (gen/map key-gen gen/any-printable-equatable)
                                initial-value (gen/one-of [gen/large-integer
                                                           (gen/elements [(byte 0) (short 0) (int 0) 0.5])])
                                limit (gen/choose 1 100)]
                   (let [actual (into initial expected)
                         ordered (into (sorted-map) expected)
                         fold (fn fold [^long result ^long id value]
                                (unchecked-add result (bit-xor id (long (hash value)))))
                         fold-long (fn fold-long ^long [^long result ^long id value]
                                     (unchecked-add result (bit-xor id (long (hash value)))))
                         visits (atom 0)
                         stopped (reduce-kv (fn [^long result ^long id value]
                                              (let [result (fold result id value)]
                                                (if (= limit (swap! visits inc)) (reduced result) result)))
                                            initial-value actual)]
                     (and (= (reduce-kv fold initial-value actual) (reduce-kv fold initial-value ordered))
                          (= (reduce-kv fold-long initial-value actual) (reduce-kv fold-long initial-value ordered))
                          (identical? initial-value (reduce-kv fold-long initial-value (empty actual)))
                          (= (min (long limit) (count expected)) @visits)
                          (= stopped (reduce-kv fold initial-value (coll/into-> ordered (sorted-map) (take limit))))))))]
    (println :numeric-property label (select-keys result [:pass? :num-tests :seed :time-elapsed-ms]))
    (assert (:pass? result))))

(defn- update-one [implementation table id function present-only]
  ((:update implementation) table id function present-only))

(defn- transformed [kind value old]
  (case (long kind)
    0 old
    1 value
    2 [value old]
    3 {:old old :generated value}
    4 nil))

(def ^:private operation-gen
  (gen/tuple (gen/choose 0 3) key-gen (gen/choose 0 4) gen/any-printable-equatable))

(defn- apply-model [table [action id kind value]]
  (case (long action)
    0 (assoc table id value)
    1 (dissoc table id)
    2 (update table id (partial transformed kind value))
    3 (if (contains? table id) (update table id (partial transformed kind value)) table)))

(defn- apply-actual [implementation table [action id kind value] editable]
  (case (long action)
    0 (if editable (assoc! table id value) (assoc table id value))
    1 (if editable (dissoc! table id) (dissoc table id))
    (let [calls (atom [])
          before (get table id)
          expected-calls (if (and (= 3 (long action)) (not (contains? table id))) [] [before])
          changed (update-one implementation table id (fn [old] (swap! calls conj old) (transformed kind value old)) (= 3 (long action)))]
      (assert (= expected-calls @calls))
      (when editable (assert (identical? table changed)))
      changed)))

(defn- check! [label property trials]
  (let [result (tc/quick-check trials property)
        report (assoc (select-keys result [:pass? :num-tests :seed :time-elapsed-ms :shrunk]) :check label)]
    (println report)
    (flush)
    (assert (:pass? result))
    report))

(defn- roundtrip [table]
  (let [bytes (ByteArrayOutputStream.)
        loader (.getClassLoader (class table))]
    (with-open [output (ObjectOutputStream. bytes)] (.writeObject output table))
    (with-open [input (proxy [ObjectInputStream] [(ByteArrayInputStream. (.toByteArray bytes))]
                        (resolveClass [stream-class]
                          (Class/forName (.getName ^ObjectStreamClass stream-class) false loader)))]
      (.readObject input))))

;; Generated updates branch from historical snapshots and mix ordinary edits,
;; callbacks and editable batches. Every historical version must remain valid.
(defn- validate-histories! [{:keys [label initial] :as implementation}]
  (check!
    [label :update-histories]
    (prop/for-all [entries (gen/vector (gen/tuple key-gen gen/any-printable-equatable))
                   steps (gen/scale #(min 35 ^long %)
                                    (gen/vector (gen/tuple gen/nat gen/boolean (gen/vector operation-gen))))]
      (let [metadata {:prototype label}
            versions (reduce
                       (fn [versions [^long parent editable operations]]
                         (let [[actual expected] (nth versions (mod parent (count versions)))
                               changed (if editable
                                         (persistent! (reduce #(apply-actual implementation %1 %2 true) (transient actual) operations))
                                         (reduce #(apply-actual implementation %1 %2 false) actual operations))]
                           (conj versions [changed (reduce apply-model expected operations)])))
                       [[(with-meta (into initial entries) metadata) (with-meta (into {} entries) metadata)]]
                       steps)]
        (coll/every? (fn [[actual expected]] (equivalent? actual expected)) versions)))
    500))

;; Clustered generated addresses cross leaf, branch and fallback boundaries.
;; Sibling editors interleave fused updates, ordinary edits and publication.
(defn- validate-regions! [{:keys [label initial] :as implementation}]
  (check!
    [label :regions-and-siblings]
    (prop/for-all [base key-gen
                   stride (gen/elements [1 32 512 8192 131072 2097152 33554432 536870912 8589934592])
                   values (gen/resize 12 (gen/vector gen/any-printable-equatable 1 12))
                   size (gen/choose 1 128)
                   actions (gen/resize 60 (gen/vector (gen/tuple gen/boolean (gen/choose 0 4) gen/nat
                                                                 (gen/choose 0 4) gen/any-printable-equatable)))]
      (let [address (fn address [^long offset] (unchecked-add (long base) (unchecked-multiply (long stride) offset)))
            entries (mapv (fn [^long offset] [(address offset) (nth values (rem offset (count values)))]) (range size))
            original (into initial entries)
            expected (into {} entries)
            state (reduce
                    (fn [{:keys [editors models versions]} [second-editor action ^long offset kind value]]
                      (let [which (if second-editor 1 0)
                            editor (nth editors which)
                            expected (nth models which)
                            id (address (rem offset (* 2 (long size))))]
                        (if (= 4 (long action))
                          (let [published (persistent! editor)]
                            {:editors (assoc editors which (transient published)) :models models
                             :versions (conj versions [published expected])})
                          (let [operation [action id kind value]
                                editor (apply-actual implementation editor operation true)
                                expected (apply-model expected operation)]
                            (assert (= (count expected) (count editor)))
                            (assert (= (get expected id ::absent) (get editor id ::absent)))
                            {:editors (assoc editors which editor) :models (assoc models which expected) :versions versions}))))
                    {:editors [(transient original) (transient original)] :models [expected expected]
                     :versions [[original expected]]}
                    actions)
            versions (into (:versions state) (mapv (fn [editor expected] [(persistent! editor) expected]) (:editors state) (:models state)))]
        (coll/every? (fn [[actual expected]] (equivalent? actual expected)) versions)))
    500))

(defn- throws? [exception-type f]
  (try (f) false (catch Throwable error (instance? exception-type error))))

;; Generated keys and values exercise exception safety, exactly-once callbacks,
;; identity no-ops, present nil, missing keys and publication invalidation.
(defn- validate-callbacks! [{:keys [label initial] :as implementation}]
  (check!
    [label :callback-contract]
    (prop/for-all [id key-gen
                   other-id key-gen
                   value gen/any-printable-equatable]
      (let [expected (assoc {} id value other-id nil)
            original (into initial expected)
            editor (transient original)
            exception (ex-info "Generated callback failure" {:id id})
            fail (fn fail [_] (throw exception))
            same (update-one implementation original id identity false)
            same-edit (update-one implementation editor id identity false)
            failed (throws? clojure.lang.ExceptionInfo #(update-one implementation editor id fail false))
            reentry (throws? IllegalStateException #(update-one implementation editor id (fn [_] (assoc! editor other-id value)) false))
            publication (throws? IllegalStateException #(update-one implementation editor id (fn [_] (persistent! editor)) false))
            reads (update-one implementation editor id (fn [old] (assert (= old (get editor id))) (assert (= (count editor) (count expected))) old) false)
            published (persistent! editor)]
        (and (identical? same original) (identical? editor same-edit) (identical? editor reads)
             failed reentry publication
             (identical? original published)
             (equivalent? original expected)
             (equivalent? published expected)
             (throws? IllegalAccessError #(update-one implementation editor id identity false))
             (throws? IllegalAccessError #(update-one implementation editor other-id identity true))
             (throws? IllegalAccessError #(get editor id))
             (throws? IllegalAccessError #(persistent! editor)))))
    300))

;; Serialization includes arrays published by editors and their ownership slots;
;; updates after restoration must preserve the deserialized and source versions.
(defn- validate-roundtrips! [{:keys [label initial] :as implementation}]
  (check!
    [label :updates-after-serialization]
    (prop/for-all [entries (gen/vector (gen/tuple key-gen gen/any-printable-equatable))
                   operations (gen/resize 35 (gen/vector operation-gen))]
      (let [expected (into {} entries)
            original (persistent! (reduce (fn [editor [id value]] (assoc! editor id value)) (transient initial) entries))
            restored (roundtrip original)
            changed (persistent! (reduce #(apply-actual implementation %1 %2 true) (transient restored) operations))
            model (reduce apply-model expected operations)]
        (and (equivalent? original expected) (equivalent? restored expected)
             (equivalent? changed model))))
    150))

;; Interleaves reads, mutations and publication in sibling editors, preserving
;; snapshots detect stale cached paths and mutation of shared data.
(defn- validate-cache! [{:keys [label initial]}]
  (let [result
        (tc/quick-check
          300
          (prop/for-all [base key-gen
                         stride (gen/elements [1 4 32 64 2048 65536 4294967296])
                         size (gen/elements [0 1 2 31 32 33 64 128 2048])
                         values (gen/resize 12 (gen/vector gen/any-printable-equatable 1 16))
                         events (gen/scale #(min 80 ^long %)
                                           (gen/vector (gen/tuple (gen/choose 0 6) gen/boolean gen/nat
                                                                  gen/any-printable-equatable)))]
            (let [address (fn address [^long offset]
                            (unchecked-add (long base) (unchecked-multiply (long stride) offset)))
                  entries (mapv (fn [^long index] [(address index) (nth values (rem index (count values)))]) (range size))
                  original (into initial entries)
                  expected (into {} entries)
                  final-state
                  (reduce
                    (fn [{:keys [editors models versions valid]} [action second-editor ^long offset value]]
                      (let [which (if second-editor 1 0)
                            editable (nth editors which)
                            model (nth models which)
                            index (rem offset (max 1 (* 2 (long size))))
                            id (address index)
                            other-id (address (bit-xor index 1))
                            valid (and valid (= (get model id ::missing) (get editable id ::missing))
                                       (= (contains? model other-id) (contains? editable other-id)))
                            model (case (long action)
                                    0 (do (assert (identical? editable (assoc! editable id value))) (assoc model id value))
                                    1 (do (assert (identical? editable (dissoc! editable id))) (dissoc model id))
                                    2 (do (assert (= (get model other-id ::missing) (get editable other-id ::missing))) model)
                                    3 (do (assert (= (get model id ::missing) (get editable id ::missing))) (assert (identical? editable (assoc! editable id value))) (assoc model id value))
                                    4 (do (assert (= (get model other-id ::missing) (get editable other-id ::missing))) (assert (identical? editable (assoc! editable id value))) (assoc model id value))
                                    5 (do (assert (= (get model id ::missing) (get editable id ::missing))) (assert (identical? editable (dissoc! editable other-id))) (assert (identical? editable (assoc! editable id value)))
                                          (assoc (dissoc model other-id) id value))
                                    model)
                            published (when (= 6 (long action)) (persistent! editable))
                            editors (if published (assoc editors which (transient published)) editors)
                            versions (if published (conj versions [published model]) versions)
                            editable (nth editors which)]
                        {:editors editors :models (assoc models which model) :versions versions
                         :valid (and valid (= (count model) (count editable))
                                     (= (get model id ::missing) (get editable id ::missing)))}))
                    {:editors [(transient original) (transient original)]
                     :models [expected expected] :versions [[original expected]] :valid true}
                    events)
                  versions (into (:versions final-state)
                                 (mapv (fn [editable model] [(persistent! editable) model])
                                       (:editors final-state) (:models final-state)))]
              (and (:valid final-state)
                   (coll/every? (fn [[actual expected]] (equivalent? actual expected)) versions)))))]
    (println :cache-property label (select-keys result [:pass? :num-tests :seed :time-elapsed-ms :shrunk]))
    (flush)
    (assert (:pass? result))))

;; Deletes through page/branch collapse, reinserts into the removed region and
;; preserves sibling snapshots; guards against stale transient deletion caches.
(defn- validate-deletions! [{:keys [label initial]}]
  (check!
    [label :deletions]
    (prop/for-all [base key-gen
                   stride (gen/elements [1 4 32 2048 131072 4294967296])
                   size (gen/elements [2 3 5 9 17 33 65 129])
                   reverse-order gen/boolean]
      (let [entries (mapv (fn [^long index]
                            [(unchecked-add (long base) (unchecked-multiply (long stride) index))
                             (when (odd? index) index)])
                          (range size))
            original (into initial entries)
            expected (into {} entries)
            sibling (transient original)
            ids (mapv first (if reverse-order (rseq entries) entries))
            state (reduce
                    (fn [{:keys [editable model versions]} id]
                      (let [other-id (first ids)
                            marker [id :reinserted]
                            editable (-> editable
                                         (dissoc! id)
                                         (dissoc! id)
                                         (assoc! id marker)
                                         (dissoc! other-id)
                                         (assoc! id marker)
                                         (dissoc! id))
                            model (dissoc model other-id id)
                            snapshot (persistent! editable)]
                        (assert (equivalent? snapshot model))
                        {:editable (transient snapshot)
                         :model model
                         :versions (conj versions [snapshot model])}))
                    {:editable (transient original) :model expected :versions [[original expected]]}
                    ids)
            local (reduce dissoc! (transient original) ids)
            persistent-deleted (reduce dissoc original ids)]
        (and (zero? (count (persistent! local)))
             (zero? (count persistent-deleted))
             (equivalent? (persistent! sibling) expected)
             (equivalent? (persistent! (:editable state)) (:model state))
             (coll/every? (fn [[actual expected]] (equivalent? actual expected)) (:versions state)))))
    200))

;; Mixed clustered/full-width keys verify ordered reducers and Java values.
;; Guards nils, read-only iteration and snapshots after transient edits.
;; Empty reductions preserve init identity, including Reduced and nested
;; Reduced values, without invoking the reducer.
(defn- validate-reductions! [{:keys [label initial]}]
  (check!
    [label :ordered-reductions]
    (prop/for-all [base key-gen
                   clustered (gen/vector (gen/tuple (gen/choose 0 65535) gen/any-printable-equatable))
                   scattered (gen/vector (gen/tuple key-gen gen/any-printable-equatable))
                   limit gen/nat]
      (let [entries (into (mapv (fn [[offset value]] [(unchecked-add (long base) (long offset)) value]) clustered) scattered)
            actual (into initial entries)
            empty-table (empty actual)
            unexpected-reducer (fn unexpected-reducer [& _]
                                 (throw (ex-info "Empty reduction invoked its reducer" {})))
            expected (vec (into (sorted-map) entries))
            expected-values (mapv second expected)
            view (.values ^Map actual)
            changed (persistent! (assoc! (transient actual) Long/MIN_VALUE ::changed))
            iterator (.iterator view)
            stopped (reduce-kv (fn [result id value]
                                 (if (= (count result) limit) (reduced result) (conj result [id value])))
                               [] actual)]
        (and (coll/every? (fn [accumulator]
                            (and (identical? accumulator (reduce unexpected-reducer accumulator empty-table))
                                 (identical? accumulator (reduce-kv unexpected-reducer accumulator empty-table))))
                          [nil false limit (Object.) (reduced [base limit]) (reduced (reduced nil))])
             (= expected-values (vec view))
             (= (count actual) (.size view))
             (= (coll/any? nil? expected-values) (.contains view nil))
             (= ::changed (get changed Long/MIN_VALUE))
             (try (.remove iterator) false (catch UnsupportedOperationException _ true))
             (= expected (reduce-kv (fn [result id value] (conj result [id value])) [] actual))
             (= (coll/into-> expected [] (take limit)) stopped)
             (nil? (reduce-kv (fn [_ _ _] nil) nil actual))
             (nil? (reduce-kv (fn [_ _ _] (reduced nil)) nil actual))
             (= expected (vec actual))
             (= (vec (reverse expected)) (vec (rseq actual))))))
    300))

;; Round trips generated snapshots and edits the deserialized map to verify
;; that serialized ownership flags never permit writes into shared arrays.
(defn- validate-serialization! [{:keys [label initial]}]
  (let [result
        (tc/quick-check
          100
          (prop/for-all [base key-gen
                         size (gen/elements [0 1 2 32 64 512 4096])
                         values (gen/resize 10 (gen/vector gen/any-printable-equatable 1 16))
                         edits (gen/vector edit-gen)]
            (let [entries (mapv (fn [^long index]
                                  [(unchecked-add (long base) index) (nth values (rem index (count values)))])
                                (range size))
                  original (into initial entries)
                  expected (into {} entries)
                  bytes (ByteArrayOutputStream.)
                  _ (with-open [output (ObjectOutputStream. bytes)] (.writeObject output original))
                  loader (.getClassLoader (class initial))
                  restored (with-open [input (proxy [ObjectInputStream] [(ByteArrayInputStream. (.toByteArray bytes))]
                                               (resolveClass [stream-class]
                                                 (Class/forName (.getName ^ObjectStreamClass stream-class) false loader)))]
                             (.readObject input))
                  changed (edit-many restored edits true)
                  model (reduce edit-one expected edits)]
              (and (equivalent? original expected)
                   (equivalent? restored expected)
                   (equivalent? changed model)))))]
    (println :serialization-property label (select-keys result [:pass? :num-tests :seed :time-elapsed-ms]))
    (flush)
    (assert (:pass? result))))

(defn- cluster-base
  ^long [^long id]
  (bit-and id -8589934592))

(defn- largest-cluster-base [expected]
  (first
    (reduce-kv (fn [[^long best-base ^long best-size :as best] ^long base ^long size]
                 (if (or (> size best-size)
                         (and (= size best-size) (< base best-base)))
                   [base size]
                   best))
               [0 0]
               (reduce-kv (fn [sizes id _] (update sizes (cluster-base id) (fnil inc 0))) {} expected))))

(def ^:private entries-gen
  (gen/let [clusters (gen/vector
                       (gen/tuple (gen/large-integer* {:min Long/MIN_VALUE :max Long/MAX_VALUE})
                                  (gen/choose 1 96))
                       1 6)
            values (gen/resize 12 (gen/vector gen/any-printable-equatable 1 8))]
    (coll/into-> clusters
      [[Long/MIN_VALUE nil] [Long/MAX_VALUE (peek values)]]
      (mapcat (fn [[base size]]
                (mapv (fn [^long offset]
                        [(bit-or (cluster-base base) offset)
                         (nth values (rem offset (count values)))])
                      (range size)))))))

(def ^:private permutations-gen
  (gen/let [entries entries-gen
            shuffled (gen/shuffle entries)]
    [entries shuffled]))

;; Generated insertion permutations preserve signed contents and snapshots
;; across clustered deletion, reinsertion and serialization.
(defn- validate-routing! [{:keys [label initial]}]
  (check!
    [label :insertion-permutations]
    (prop/for-all [[entries shuffled] permutations-gen]
      (let [expected (into {} entries)
            orders [entries (vec (rseq entries)) shuffled]
            versions (coll/into-> orders []
                       (mapcat (fn [ordered]
                                 [(reduce (fn [table [id value]] (assoc table id value)) initial ordered)
                                  (persistent! (reduce (fn [editor [id value]] (assoc! editor id value))
                                                       (transient initial) ordered))])))
            removed-base (largest-cluster-base expected)
            removed (filterv (fn [[id _]] (= (cluster-base id) removed-base)) entries)
            remaining (reduce (fn [table [id _]] (dissoc table id)) expected removed)]
        (coll/every?
          (fn [original]
            (let [editor (transient original)
                  sibling (transient original)
                  _ (run! (fn [[id _]] (dissoc! editor id)) removed)
                  reduced-version (persistent! editor)
                  _ (run! (fn [[id value]] (assoc! sibling id value)) (rseq removed))
                  same-version (persistent! sibling)
                  restored (roundtrip reduced-version)
                  reinserted (persistent!
                               (reduce (fn [editor [id value]]
                                         (assoc! editor id value))
                                       (transient restored)
                                       (rseq removed)))]
              (and (equivalent? original expected)
                   (equivalent? same-version expected)
                   (equivalent? reduced-version remaining)
                   (equivalent? restored remaining)
                   (equivalent? reinserted expected))))
          versions)))
    250))

;; Verifies persistent histories, metadata, ordering and editable batches.
(deftest variant-test
  (is (do (validate-variant! long-map) true)))

;; Verifies full-width sparse keys survive splits and historical edits.
(deftest sparse-test
  (is (do (validate-sparse! long-map) true)))

;; Verifies primitive reductions preserve accumulator identity and early exit.
(deftest numeric-test
  (is (do (validate-numeric! long-map) true)))

;; Verifies sibling transients and published snapshots never share mutations.
(deftest cache-test
  (is (do (validate-cache! long-map) true)))

;; Verifies deletion, collapse and reinsertion preserve sibling snapshots.
(deftest deletions-test
  (is (do (validate-deletions! long-map) true)))

;; Verifies ordered reducers, nil values and empty Reduced accumulators.
(deftest reductions-test
  (is (do (validate-reductions! long-map) true)))

;; Verifies deserialized snapshots remain isolated during transient edits.
(deftest serialization-test
  (is (do (validate-serialization! long-map) true)))

;; Verifies insertion permutations preserve contents, order and snapshots.
(deftest routing-test
  (is (do (validate-routing! long-map) true)))

;; Verifies fused updates preserve every historical map version.
(deftest histories-test
  (is (do (validate-histories! long-map) true)))

;; Verifies clustered fused updates across sibling transient boundaries.
(deftest regions-test
  (is (do (validate-regions! long-map) true)))

;; Verifies callback exception safety, exactly-once calls and invalidation.
(deftest callbacks-test
  (is (do (validate-callbacks! long-map) true)))

;; Verifies fused updates after serialization preserve source snapshots.
(deftest roundtrips-test
  (is (do (validate-roundtrips! long-map) true)))
