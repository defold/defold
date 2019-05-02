(ns internal.history-test
  (:require [clojure.core.cache :as cc]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.system :as is]
            [support.test-support :refer [with-clean-system]]))

(defn- interpose-spaces
  ^String [^String string]
  (String. (char-array (interpose \space string))))

(g/defnode TestResourceNode
  (property property g/Str (default "default"))
  (output output g/Str (g/fnk [property] (.toUpperCase property)))
  (output cached-output g/Str :cached (g/fnk [property] (.toUpperCase property))))

(g/defnode TestConsumerNode
  (input input g/Str)
  (output output g/Str (g/fnk [input] (interpose-spaces input)))
  (output cached-output g/Str :cached (g/fnk [input] (interpose-spaces input))))

(defn make-graph-with-history! []
  (g/make-graph! :history true :volatility 1))

(defn- history-length [graph-id]
  (count (is/graph-history @g/*the-system* graph-id)))

(defn- cache-contains? [node-id output-label]
  (let [cache (is/system-cache @g/*the-system*)]
    (cc/has? cache [node-id output-label])))

(deftest cache-pollution-test
  (testing "Eval output, undo set property."
    (with-clean-system
      (let [graph-id (make-graph-with-history!)
            node-id (g/make-node! graph-id TestResourceNode)]

        (is (= "DEFAULT" (g/node-value node-id :cached-output)))
        (g/set-property! node-id :property "New Value")
        (is (= "NEW VALUE" (g/node-value node-id :cached-output)))
        (is (cache-contains? node-id :cached-output))

        (g/undo! graph-id) ; Undo set property.
        (is (not (cache-contains? node-id :cached-output)))
        (is (= "DEFAULT" (g/node-value node-id :cached-output))))))

  (testing "Eval output, undo set property, undo make node, redo make node."
    (with-clean-system
      (let [graph-id (make-graph-with-history!)
            node-id (g/make-node! graph-id TestResourceNode)]

        (is (= "DEFAULT" (g/node-value node-id :cached-output)))
        (g/set-property! node-id :property "New Value")
        (is (= "NEW VALUE" (g/node-value node-id :cached-output)))
        (is (cache-contains? node-id :cached-output))

        (g/undo! graph-id) ; Undo set property.
        (g/undo! graph-id) ; Undo make node.
        (g/redo! graph-id) ; Redo make node.
        (is (not (cache-contains? node-id :cached-output)))
        (is (= "DEFAULT" (g/node-value node-id :cached-output))))))

  (testing "Eval output, undo make node, redo make node."
    (with-clean-system
      (let [graph-id (make-graph-with-history!)
            node-id (g/make-node! graph-id TestResourceNode)]

        (is (= "DEFAULT" (g/node-value node-id :cached-output)))
        (is (cache-contains? node-id :cached-output))

        (g/undo! graph-id) ; Undo make node.
        (g/redo! graph-id) ; Redo make node.
        (is (not (cache-contains? node-id :cached-output)))
        (is (= "DEFAULT" (g/node-value node-id :cached-output))))))

  (testing "Eval output, undo make node."
    ;; It is fine for node entries to still exist in the cache, but we
    ;; prefer to have the unused structures be garbage collected.
    #_(with-clean-system
        (let [graph-id (make-graph-with-history!)
              node-id (g/make-node! graph-id TestResourceNode)]

          (is (= "DEFAULT" (g/node-value node-id :cached-output)))
          (is (cache-contains? node-id :cached-output))

          (g/undo! graph-id) ; Undo make node.
          (is (not (cache-contains? node-id :cached-output)))))))

(deftest can-undo-redo-test
  (with-clean-system

    (let [graph-id (make-graph-with-history!)]
      (is (false? (g/can-undo? graph-id)))
      (is (false? (g/can-redo? graph-id)))

      (let [node-id (g/make-node! graph-id TestResourceNode)]
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

(deftest invalidation-across-graphs-test
  (with-clean-system
    (let [project-graph-id (g/make-graph! :history true)
          view-graph-id (g/make-graph! :volatility 100)
          resource-node-id (g/make-node! project-graph-id TestResourceNode)
          view-node-id (g/make-node! view-graph-id TestConsumerNode)]

      (g/connect! resource-node-id :cached-output view-node-id :input)
      (is (= "DEFAULT" (g/node-value resource-node-id :cached-output)))
      (is (= "D E F A U L T" (g/node-value view-node-id :cached-output)))

      (g/set-property! resource-node-id :property "changed")
      (is (= "CHANGED" (g/node-value resource-node-id :cached-output)))
      (is (= "C H A N G E D" (g/node-value view-node-id :cached-output)))

      (g/undo! project-graph-id)
      (is (= "DEFAULT" (g/node-value resource-node-id :cached-output)))
      (is (= "D E F A U L T" (g/node-value view-node-id :cached-output))))))

(deftest reset-history-test
  (with-clean-system

    ;; History should contain a single entry for a newly created graph.
    (let [graph-id (make-graph-with-history!)]
      (is (= 1 (history-length graph-id)))

      ;; Create a node, ensure history grows.
      (let [node-id (g/make-node! graph-id TestResourceNode)]
        (is (= 2 (history-length graph-id)))
        (is (some? (g/node-by-id node-id)))

        ;; Evaluate cached output to enter it into the cache.
        (is (not (cache-contains? node-id :cached-output)))
        (is (= "DEFAULT" (g/node-value node-id :cached-output)))
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
        #_(is (not (cache-contains? node-id :cached-output)))

        ;; User-data persists outside history.
        (is (= "test-user-data" (g/user-data node-id :test-user-data)))))))

(deftest simple-undo-redo-test
  (with-clean-system

    (let [graph-id (make-graph-with-history!)
          node-id (g/make-node! graph-id TestResourceNode)
          node-exists? #(some? (g/node-by-id node-id))
          set-property! #(g/set-property! node-id :property %)
          eval-output #(g/node-value node-id :cached-output)]

      (is (= "DEFAULT" (eval-output)))
      (set-property! "New Value")
      (is (= "NEW VALUE" (eval-output)))

      (g/undo! graph-id)
      (is (= "DEFAULT" (eval-output)))

      (g/undo! graph-id)
      (is (not (node-exists?)))

      (g/redo! graph-id)
      (is (node-exists?))

      (is (= "DEFAULT" (eval-output)))
      (g/redo! graph-id)
      (is (= "NEW VALUE" (eval-output)))

      (g/undo! graph-id)
      (is (= "DEFAULT" (eval-output)))

      (set-property! "Newer Value")
      (is (= "NEWER VALUE" (eval-output)))
      (is (false? (g/can-redo? graph-id))))))