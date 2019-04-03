(ns support.test-support
  (:require [dynamo.graph :as g]
            [editor.fs :as fs]
            [clojure.java.io :as io]
            [internal.system :as is]))

(defmacro with-clean-system
  [& forms]
  (let [configuration (if (map? (first forms)) (first forms) {:cache-size 1000})
        forms (if (map? (first forms)) (next forms)  forms)]
    `(let [system# (is/make-system ~configuration)
           ~'cache (is/system-cache system#)
           ~'world (first (keys (is/graphs system#)))]
       (binding [g/*the-system* (atom system#)]
         ~@forms))))

(defn tx-nodes [& txs]
  (g/tx-nodes-added (g/transact txs)))

(defn array= [a b]
  (and
    (= (class a) (class b))
    (= (count a) (count b))
    (every? true? (map = a b))))

(defn yield
  "Give up the thread just long enough for a context switch"
  []
  (Thread/sleep 1))

(defn can-undo? [graph-id]
  (g/can-undo? graph-id))

(defn undo! [graph-id]
  (g/undo! graph-id))

(defn can-redo? [graph-id]
  (g/can-redo? graph-id))

(defn redo! [graph-id]
  (g/redo! graph-id))

;; These *-until-new-mtime fns are hacks to support the resource-watch sync, which checks mtime

(defn write-until-new-mtime [write-fn f & args]
  (let [f (io/as-file f)
        mtime (.lastModified f)]
    (loop []
      (apply write-fn f args)
      (let [new-mtime (.lastModified f)]
        (when (= mtime new-mtime)
          (Thread/sleep 50)
          (recur))))))

(defn spit-until-new-mtime [f content & args]
  (apply write-until-new-mtime spit f content args))

(defn touch-until-new-mtime [f]
  (write-until-new-mtime (fn [f] (fs/touch-file! f)) f))

(defn graph-dependencies
  ([tgts]
   (graph-dependencies (g/now) tgts))
  ([basis tgts]
   (->> tgts
        (reduce (fn [m [nid l]]
                  (update m nid (fn [s l] (if s (conj s l) #{l})) l))
                {})
        (g/dependencies basis)
        (into #{} (mapcat (fn [[nid ls]] (mapv #(vector nid %) ls)))))))
