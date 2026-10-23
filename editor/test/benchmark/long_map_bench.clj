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

(ns benchmark.long-map-bench
  "Local screening: three implementations, fixed inputs and unprofiled timings."
  (:require [clj-async-profiler.core :as profiler]
            [clojure.data.int-map :as int-map]
            [clojure.java.io :as io]
            [criterium.core :as criterium]
            [util.coll :as coll]
            [util.defonce :as defonce])
  (:import [com.defold.util LongMap LongMap$Transient]
           [com.sun.management ThreadMXBean]
           [java.io File]
           [java.lang ClassValue]
           [java.lang.instrument Instrumentation]
           [java.lang.management ManagementFactory]
           [java.lang.reflect Array Field Modifier]
           [java.util ArrayDeque IdentityHashMap Map Random]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/record Node [^long id ^long revision properties])

(def scenarios
  [[:dense-lookup :dense :lookup 4096]
   [:fragmented-lookup :fragmented :lookup 4096]
   [:sparse-lookup :sparse :lookup 4096]
   [:sparse-persistent-edit :sparse :history 64]
   [:sparse-editable-batch :sparse :batch 1024]
   [:sparse-persistent-delete :sparse :delete 1024]
   [:sparse-editable-delete :sparse :delete-batch 1024]
   [:sparse-build :sparse :build 100000]
   [:sparse-reduction :sparse :reduce 100000]
   [:outlier-first-lookup :outlier-first :lookup 4096]
   [:outlier-last-lookup :outlier-last :lookup 4096]
   [:persistent-edit :dense :history 64]
   [:persistent-replace :dense :replace 64]
   [:small-editable-batch :dense :batch 8]
   [:random-editable-batch :dense :batch 1024]
   [:local-editable-batch :dense :local 1024]
   [:persistent-delete :dense :delete 1024]
   [:editable-delete :dense :delete-batch 1024]
   [:absent-persistent-delete :dense :delete-absent 1024]
   [:absent-editable-delete :dense :delete-absent-batch 1024]
   [:local-persistent-delete :dense :delete-local 1024]
   [:local-editable-delete :dense :delete-local-batch 1024]
   [:fragmented-persistent-delete :fragmented :delete 1024]
   [:fragmented-editable-delete :fragmented :delete-batch 1024]
   [:build :dense :build 100000]
   [:contiguous-append :dense :append 8192]
   [:distant-append :dense :append-distant 8192]
   [:reduction :dense :reduce 100000]
   [:fused-history :dense :update-history 64]
   [:fused-random-batch :dense :update-batch 1024]
   [:fused-local-batch :dense :update-local 1024]
   [:typed-reduction :dense :reduce-long 100000]
   [:dense-values :dense :values 100000]
   [:sparse-values :sparse :values 100000]
   [:tile-values :tile :values 100000]
   [:tile-lookup :tile :lookup 4096]
   [:tile-paint :tile :local 1024]
   [:tile-erase :tile :delete-local-batch 1024]
   [:tile-mixed :tile :mixed 1024]])

(defn- initial [implementation]
  (case implementation
    :clojure {}
    :int-map (int-map/int-map)
    :long-map LongMap/EMPTY))

(defn- bump [^Node node]
  (->Node (.-id node) (inc (.-revision node)) (.-properties node)))

(defn- update-one [table ^long id]
  (cond
    (instance? LongMap table) (.update ^LongMap table id bump)
    (instance? LongMap$Transient table) (.update ^LongMap$Transient table id bump)
    :else (if (instance? clojure.lang.ITransientMap table)
            (assoc! table id (bump (get table id)))
            (update table id bump))))

(defn- fixture [[label distribution operation batch] implementation]
  (let [random (Random. 424242)
        ids (mapv (fn [^long index]
                    (case distribution
                      :sparse (.nextLong random)
                      :tile (bit-or (bit-shift-left (- (quot index 400) 125) 32)
                                    (bit-and (- (rem index 400) 200) 0xffffffff))
                      :fragmented (+ 4294967296 (* 4 index))
                      (+ 4294967296 index)))
                  (range 100000))
        nodes (mapv (fn [id] [id (->Node id 0 {:enabled true})]) ids)
        outlier [Long/MIN_VALUE (->Node Long/MIN_VALUE 0 {})]
        entries (case distribution
                  :outlier-first (into [outlier] nodes)
                  :outlier-last (conj nodes outlier)
                  nodes)
        queries (object-array (mapv (fn [_] (nth ids (.nextInt random (count ids)))) (range batch)))
        local-ids (object-array
                    (if (= :tile distribution)
                      (mapv (fn [^long index]
                              (let [x (+ (if (= :mixed operation) 180 -16) (rem index 32))
                                    y (+ (if (= :mixed operation) 110 -16) (quot index 32))]
                                (bit-or (bit-shift-left y 32) (bit-and x 0xffffffff))))
                            (range batch))
                      (subvec ids 0 batch)))
        start (if (= :append-distant operation) 8589934592 4295067296)
        additions (mapv (fn [^long offset]
                          (let [id (+ (long start) offset)]
                            [id (->Node id 0 {:enabled true})]))
                        (range batch))
        table (into (initial implementation) entries)]
    {:label label :operation operation :batch batch :table table :entries entries
     :queries queries :local-ids local-ids :additions additions
     :absent-ids (object-array (mapv first additions))}))

(defn- read-batch [table ^objects ids]
  (loop [index 0
         result 0]
    (if (= index (alength ids))
      result
      (let [^Node node (get table (aget ids index))]
        (recur (inc index) (unchecked-add result (.-id node)))))))

(defn- edit-batch [table ^objects ids fused remove-keys]
  (loop [index 0
         editor (transient table)]
    (if (= index (alength ids))
      (persistent! editor)
      (let [id (aget ids index)]
        (recur (inc index)
               (cond
                 remove-keys (dissoc! editor id)
                 fused (update-one editor id)
                 :else (assoc! editor id index)))))))

(defn- history [table ^objects ids fused remove-keys]
  (loop [index 0
         table table
         versions []]
    (if (= index (alength ids))
      versions
      (let [id (aget ids index)
            table (cond
                    remove-keys (dissoc table id)
                    fused (update-one table id)
                    :else (assoc table id index))]
        (recur (inc index) table (conj versions table))))))

(defn- read-values [^Map table]
  (let [iterator (.iterator (.values table))]
    (loop [result 0]
      (if-not (.hasNext iterator)
        result
        (let [^Node node (.next iterator)]
          (recur (unchecked-add result (.-id node))))))))

(defn- edit-mixed [table ^objects ids]
  (loop [index 0
         editor (transient table)]
    (if (= index (alength ids))
      (persistent! editor)
      (let [id (aget ids index)]
        (recur (inc index)
               (if (zero? (bit-and index 3))
                 (dissoc! editor id)
                 (assoc! editor id index)))))))

(defn- workload [{:keys [table entries queries local-ids additions absent-ids operation]}]
  (case operation
    :lookup (fn lookup [] (read-batch table queries))
    :values (fn values [] (read-values table))
    :mixed (fn mixed-brush [] (edit-mixed table local-ids))
    :reduce-long (fn reduce-longs [] (reduce-kv (fn sum ^long [^long total ^long id _] (unchecked-add total id)) 0 table))
    :history (fn persistent-edits [] (history table queries false false))
    :replace (fn persistent-replacements []
               (reduce (fn replace-node [table id] (assoc table id :edited)) table queries))
    :delete (fn persistent-deletions [] (reduce dissoc table queries))
    :batch (fn editable-edits [] (edit-batch table queries false false))
    :local (fn editable-local-edits [] (edit-batch table local-ids false false))
    :delete-batch (fn editable-deletions [] (edit-batch table queries false true))
    :delete-absent (fn persistent-absent-deletions [] (reduce dissoc table absent-ids))
    :delete-absent-batch (fn editable-absent-deletions [] (edit-batch table absent-ids false true))
    :delete-local (fn persistent-local-deletions [] (reduce dissoc table local-ids))
    :delete-local-batch (fn editable-local-deletions [] (edit-batch table local-ids false true))
    :build (fn construction [] (into (empty table) entries))
    (:append :append-distant) (fn append-nodes [] (into table additions))
    :reduce (fn reduce-nodes [] (reduce-kv (fn sum [^long total ^long id _] (unchecked-add total id)) 0 table))
    :update-history (fn fused-history [] (history table queries true false))
    :update-batch (fn fused-batch [] (edit-batch table queries true false))
    :update-local (fn fused-local-batch [] (edit-batch table local-ids true false))))

(defn- resource-usage [work ^long operations]
  (let [^ThreadMXBean bean (ManagementFactory/getThreadMXBean)
        thread (.threadId (Thread/currentThread))
        before-bytes (.getThreadAllocatedBytes bean thread)
        before-cpu (.getCurrentThreadCpuTime bean)
        before-wall (System/nanoTime)]
    (dotimes [_ 20]
      (criterium/set-place criterium/mutable-place (work)))
    (let [elapsed-wall (- (System/nanoTime) before-wall)
          elapsed-cpu (- (.getCurrentThreadCpuTime bean) before-cpu)
          bytes (- (.getThreadAllocatedBytes bean thread) before-bytes)
          divisor (* 20.0 operations)]
      {:bytes-per-operation (/ (double bytes) divisor)
       :thread-cpu-ns-per-operation (/ (double elapsed-cpu) divisor)
       :resource-wall-ns-per-operation (/ (double elapsed-wall) divisor)})))

(defn- measure! [scenario implementation output run]
  (let [fixture (fixture scenario implementation)
        work (workload fixture)
        reference (workload (assoc fixture :table (into {} (:entries fixture))))
        _ (assert (= (reference) (work)))
        options (merge criterium/*default-benchmark-opts*
                       {:samples (long (Long/getLong "long-map.samples" (long 8)))
                        :warmup-jit-period (long (Long/getLong "long-map.warmup-ns" (long 750000000)))
                        :target-execution-time (long (Long/getLong "long-map.sample-ns" (long 20000000)))
                        :bootstrap-size 500
                        :overhead 0
                        :gc-before-sample false})
        samples (criterium/run-benchmark (:samples options) (:warmup-jit-period options)
                                         (:target-execution-time options) work false 0)
        stats (criterium/benchmark-stats samples options)
        operations (long (:batch fixture))
        row (merge (resource-usage work operations)
                   {:scenario (:label fixture)
                    :implementation implementation
                    :run run
                    :ns-per-operation (/ (* 1.0e9 (double (first (:mean stats)))) operations)
                    :samples (dissoc samples :results)})]
    (spit output (str (pr-str row) "\n") :append true)
    (prn (dissoc row :samples))
    (flush)))

(defn- run-for! [work ^long nanoseconds]
  (let [end (+ (System/nanoTime) nanoseconds)]
    (loop [iterations 0]
      (if (< (System/nanoTime) end)
        (do
          (criterium/set-place criterium/mutable-place (work))
          (recur (inc iterations)))
        iterations))))

(defn profile! [implementation scenario-name event directory]
  (let [scenario (coll/first-where #(= scenario-name (first %)) scenarios)]
    (when-not scenario
      (throw (ex-info "Unknown profiling scenario" {:scenario scenario-name})))
    (let [fixture (fixture scenario implementation)
          work (workload fixture)
          reference (workload (assoc fixture :table (into {} (:entries fixture))))
          prefix (io/file directory (str (name scenario-name) "-" (name implementation) "-" (name event)))]
      (assert (= (reference) (work)))
      (.mkdirs (io/file directory))
      (run-for! work 5000000000)
      (profiler/start {:event event
                       :interval (if (= :alloc event) 65536 1000000)})
      (try
        (run-for! work 10000000000)
        (finally
          (let [^File stacks (profiler/stop {:generate-flamegraph? false})
                ^File flamegraph (profiler/generate-flamegraph stacks {:title (.getName prefix)})]
            (io/copy stacks (io/file (str prefix ".txt")))
            (io/copy flamegraph (io/file (str prefix ".html")))
            (prn {:scenario scenario-name :implementation implementation :event event :profile (str prefix)})
            (flush)))))))

(def ^:private reference-fields
  (proxy [ClassValue] []
    (computeValue [type]
      (loop [^Class type type
             fields []]
        (if-not type
          fields
          (recur (.getSuperclass type)
                 (coll/into-> (.getDeclaredFields type) fields
                   (comp (remove (fn [^Field field]
                                   (or (Modifier/isStatic (.getModifiers field))
                                       (.isPrimitive (.getType field)))))
                         (map (fn [^Field field]
                                (.setAccessible field true)
                                field))))))))))

(defn- footprint [^Instrumentation instrumentation roots]
  (let [visited (IdentityHashMap.)
        pending (ArrayDeque. ^java.util.Collection roots)]
    (loop [bytes 0
           objects 0]
      (if (.isEmpty pending)
        {:bytes bytes :objects objects}
        (let [value (.removeLast pending)]
          (if (or (instance? Node value) (.put visited value true))
            (recur bytes objects)
            (let [type (class value)]
              (if (.isArray type)
                (when-not (.isPrimitive (.getComponentType type))
                  (dotimes [index (Array/getLength value)]
                    (when-some [reference (Array/get value index)]
                      (.add pending reference))))
                (doseq [^Field field (.get ^ClassValue reference-fields type)]
                  (when-some [reference (.get field value)]
                    (.add pending reference))))
              (recur (+ bytes (.getObjectSize instrumentation value))
                     (inc objects)))))))))

(defn memory! [output]
  ;; Jamm supplies the standard Instrumentation agent. Walking references here
  ;; preserves payload exclusion and counts shared objects once across roots.
  (let [instrumentation (.get (.getField (Class/forName "org.github.jamm.strategies.MemoryMeterStrategies")
                                         "instrumentation") nil)]
    (when-not instrumentation
      (throw (ex-info "Memory measurements require the Jamm javaagent" {})))
    (doseq [distribution [:dense :fragmented :sparse :tile]
            implementation [:clojure :int-map :long-map]
            retain-history [false true]]
      (let [{:keys [table queries]} (fixture [:memory distribution :history 64] implementation)
            roots (if retain-history (into [table] (history table queries true false)) [table])
            footprint (footprint instrumentation roots)
            row {:distribution distribution
                 :implementation implementation
                 :history retain-history
                 :bytes (:bytes footprint)
                 :objects (:objects footprint)}]
        (spit output (str (pr-str row) "\n") :append true)
        (prn row)
        (flush)))))

(defn -main [& args]
  (cond
    (= "profile" (first args))
    (profile! (keyword (nth args 1)) (keyword (nth args 2)) (keyword (nth args 3)) (nth args 4))

    (= "memory" (first args))
    (memory! (second args))

    :else
    (let [implementation (keyword (first args))
          output (second args)
          run (Long/parseLong (nth args 2))
          selected (coll/into-> (drop 3 args) #{} (map keyword))]
      (doseq [scenario (if (zero? (count selected)) scenarios (filterv #(contains? selected (first %)) scenarios))]
        (when (Boolean/getBoolean "long-map.controlled")
          (prn {:benchmark :ready :scenario (first scenario)})
          (flush)
          (assert (= "go" (read-line))))
        (measure! scenario implementation output run)
        (prn {:benchmark :done :scenario (first scenario)})
        (flush))))
  (shutdown-agents))
