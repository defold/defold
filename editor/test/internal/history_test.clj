(ns internal.history-test
  (:require [clojure.core.cache :as cc]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.system :as is]
            [support.test-support :refer [with-clean-system]]))

(g/defnode TestNode
  (property title g/Str (default "untitled"))
  (output cached-output g/Any :cached (g/fnk [title] (.toUpperCase title))))

(defn make-graph-with-history! []
  (g/make-graph! :history true :volatility 1))

(defn- history-length [graph-id]
  (count (is/graph-history @g/*the-system* graph-id)))

(defn- cache-contains? [node-id output-label]
  (let [cache (is/system-cache @g/*the-system*)]
    (cc/has? cache [node-id output-label])))

(deftest reset-history-test
 (with-clean-system

    ;; History should contain a single entry for a newly created graph.
    (let [graph-id (make-graph-with-history!)]
      (is (= 1 (history-length graph-id)))

      ;; Create a node, ensure history grows.
      (let [node-id (g/make-node! graph-id TestNode)]
        (is (= 2 (history-length graph-id)))
        (is (some? (g/node-by-id node-id)))

        ;; Evaluate cached output to enter it into the cache.
        (is (not (cache-contains? node-id :cached-output)))
        (is (= "UNTITLED" (g/node-value node-id :cached-output)))
        (is (cache-contains? node-id :cached-output))

        ;; Add user-data to the node.
        (g/user-data! node-id :test-user-data "test-user-data")
        (is (= "test-user-data" (g/user-data node-id :test-user-data)))

        ;; Reset history.
        (g/reset-history! graph-id 0)

        ;; History should be cut off and the node should no longer exist.
        (is (= 1 (history-length graph-id)))
        (is (nil? (g/node-by-id node-id)))

        ;; It is fine for node entries to still exist in the cache, but we
        ;; prefer to have the unused structures be garbage collected.
        ;; TODO: This might be critical now that node-ids are reused? Investigate.
        #_(is (not (cache-contains? node-id :cached-output)))

        ;; User-data persists outside history.
        (is (= "test-user-data" (g/user-data node-id :test-user-data)))))))

(deftest simple-undo-redo-test
  (with-clean-system

    (let [graph-id (make-graph-with-history!)
          node-id (g/make-node! graph-id TestNode)
          node-exists? #(some? (g/node-by-id node-id))
          set-title! #(g/set-property! node-id :title %)
          eval-output #(g/node-value node-id :cached-output)]

      (is (= "UNTITLED" (eval-output)))
      (set-title! "New Title")
      (is (= "NEW TITLE" (eval-output)))

      (g/undo! graph-id)
      (is (= "UNTITLED" (eval-output)))

      (g/undo! graph-id)
      (is (not (node-exists?)))

      (g/redo! graph-id)
      (is (node-exists?))

      (is (= "UNTITLED" (eval-output)))
      (g/redo! graph-id)
      (is (= "NEW TITLE" (eval-output)))

      (g/undo! graph-id)
      (is (= "UNTITLED" (eval-output)))

      (set-title! "Newer Title")
      (is (= "NEWER TITLE" (eval-output)))
      (is (false? (g/can-redo? graph-id))))))

(deftest can-undo-redo-test
  (with-clean-system

    (let [graph-id (make-graph-with-history!)]
      (is (false? (g/can-undo? graph-id)))
      (is (false? (g/can-redo? graph-id)))

      (let [node-id (g/make-node! graph-id TestNode)]
        (is (some? (g/node-by-id node-id)))
        (is (true? (g/can-undo? graph-id)))
        (is (false? (g/can-redo? graph-id)))

        (g/undo! graph-id)
        (is (nil? (g/node-by-id node-id)))
        (is (false? (g/can-undo? graph-id)))
        (is (true? (g/can-redo? graph-id)))

        (g/redo! graph-id)
        (is (some? (g/node-by-id node-id)))
        (is (true? (g/can-undo? graph-id)))
        (is (false? (g/can-redo? graph-id)))

        (g/undo! graph-id)
        (is (nil? (g/node-by-id node-id)))
        (is (false? (g/can-undo? graph-id)))
        (is (true? (g/can-redo? graph-id)))))))
