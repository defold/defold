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

(ns editor.handler-test
  (:require [clojure.test :refer :all :exclude [run-test]]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.handler :as handler]
            [editor.localization :as localization]
            [integration.test-util :as test-util]
            [service.log :as log]
            [support.test-support :refer [tx-nodes with-clean-system]]
            [util.fn :as fn])
  (:import [clojure.lang Keyword]))

(defn fixture [f]
  (with-redefs [handler/state-atom (atom handler/empty-state)]
    (f)))

(use-fixtures :each fixture)

(deftest run-test
  (with-clean-system
    (handler/defhandler :file.open :global
      (enabled? [instances] (every? #(= % :foo) instances))
      (run [instances] 123))
    (are [inst exp] (= exp (test-util/handler-enabled? :file.open [(handler/->context :global {:instances [inst]})] {}))
         :foo true
         :bar false)
    (is (= 123 (test-util/handler-run :file.open [(handler/->context :global {:instances [:foo]})] {})))))

(deftest context
  (with-clean-system
    (handler/defhandler :c1 :global
      (active? [global-context] true)
      (enabled? [global-context] true)
      (run [global-context]
           (when global-context
             :c1)))

    (handler/defhandler :c2 :local
      (active? [local-context] true)
      (enabled? [local-context] true)
      (run [local-context]
           (when local-context
             :c2)))

    (let [global-context (handler/->context :global {:global-context true})
          local-context (handler/->context :local {:local-context true})]
      (is (test-util/handler-enabled? :c1 [local-context global-context] {}))
      (is (not (test-util/handler-enabled? :c1 [local-context] {})))
      (is (test-util/handler-enabled? :c2 [local-context global-context] {}))
      (is (not (test-util/handler-enabled? :c2 [global-context] {}))))))

(defrecord StaticSelection [selection]
  handler/SelectionProvider
  (selection [_this _evaluation-context] selection)
  (succeeding-selection [_this _evaluation-context] [])
  (alt-selection [_this _evaluation-context] []))

(defrecord DynamicSelection [selection-ref]
  handler/SelectionProvider
  (selection [_this _evaluation-context] @selection-ref)
  (succeeding-selection [_this _evaluation-context] [])
  (alt-selection [_this _evaluation-context] []))

(extend-type java.lang.String
  core/Adaptable
  (adapt [this t _evaluation-context]
    (cond
      (= t Keyword) (keyword this))))

(deftest selection-test
  (with-clean-system
    (let [global (handler/->context :global {:global-context true} (->StaticSelection [:a]) {})]
      (handler/defhandler :c1 :global
        (active? [selection] selection)
        (run [selection]
             (g/with-auto-evaluation-context evaluation-context
               (handler/adapt selection Keyword evaluation-context))))
      (doseq [[local-selection expected-selection] [[["b"] [:b]]
                                                    [[] []]
                                                    [nil [:a]]]]
        (let [local (handler/->context :local
                                       {:local-context true}
                                       (when local-selection
                                         (->StaticSelection local-selection))
                                       [])]
          (is (= expected-selection (test-util/handler-run :c1 [local global] {}))))))))

(deftest selection-context
  (with-clean-system
    (let [[global local] (mapv #(handler/->context % {} (->StaticSelection [:a]) {}) [:global :local])]
      (handler/defhandler :c1 :global
        (active? [selection selection-context] (and (= :global selection-context) selection))
        (run [selection]
             nil))
      (is (test-util/handler-enabled? :c1 [global] {}))
      (is (not (test-util/handler-enabled? :c1 [local] {})))
      (is (test-util/handler-enabled? :c1 [local global] {})))))

(deftest erroneous-handler
  (with-clean-system
    (let [global (handler/->context :global {:global-context true} (->StaticSelection [:a]) {})]
      (handler/defhandler :erroneous :global
        (active? [does-not-exist] true)
        (enabled? [does-not-exist] true)
        (run [does-not-exist] (throw (Exception. "should never happen"))))
      (log/without-logging
        (is (not (test-util/handler-enabled? :erroneous [global] {})))
        (is (nil? (test-util/handler-run :erroneous [global] {})))))))

(deftest throwing-handler
  (with-clean-system
    (let [global (handler/->context :global {:global-context true} (->StaticSelection [:a]) {})
          throwing-enabled? (fn/make-call-logger (fn [selection] (throw (Exception. "Thrown from enabled?"))))
          throwing-run (fn/make-call-logger (fn [selection] (throw (Exception. "Thrown from run"))))]
      (handler/enable-disabled-handlers!)
      (handler/defhandler :throwing :global
        (active? [selection] true)
        (enabled? [selection] (throwing-enabled? selection))
        (run [selection] (throwing-run selection)))
      (log/without-logging
        (testing "The enabled? function will not be called anymore if it threw an exception."
          (is (not (test-util/handler-enabled? :throwing [global] {})))
          (is (not (test-util/handler-enabled? :throwing [global] {})))
          (is (= 1 (count (fn/call-logger-calls throwing-enabled?)))))
        (testing "The command can be repeated even though an exception was thrown during run."
          (is (nil? (test-util/handler-run :throwing [global] {})))
          (is (nil? (test-util/handler-run :throwing [global] {})))
          (is (= 2 (count (fn/call-logger-calls throwing-run)))))
        (testing "Disabled handlers can be re-enabled during development."
          (is (= 1 (count (fn/call-logger-calls throwing-enabled?))))
          (test-util/handler-enabled? :throwing [global] {})
          (is (= 1 (count (fn/call-logger-calls throwing-enabled?))))
          (handler/enable-disabled-handlers!)
          (test-util/handler-enabled? :throwing [global] {})
          (is (= 2 (count (fn/call-logger-calls throwing-enabled?)))))))))

(defprotocol AProtocol)

(defrecord ARecord []
  AProtocol)

(deftest adaptables
  (with-clean-system
    (is (not-empty
          (keep identity
                (g/with-auto-evaluation-context evaluation-context
                  (handler/adapt [(->ARecord)] AProtocol evaluation-context)))))))

(g/defnode StringNode
  (property string g/Str))

(g/defnode IntNode
  (property int g/Int))

(defrecord OtherType [])

(deftest adapt-nodes
  (handler/defhandler :string-command :global
    (enabled? [selection evaluation-context] (handler/adapt-every selection String evaluation-context))
    (run [selection] (g/with-auto-evaluation-context evaluation-context
                       (handler/adapt-every selection String evaluation-context))))
  (handler/defhandler :single-string-command :global
    (enabled? [selection evaluation-context] (handler/adapt-single selection String evaluation-context))
    (run [selection] (g/with-auto-evaluation-context evaluation-context
                       (handler/adapt-single selection String evaluation-context))))
  (handler/defhandler :int-command :global
    (enabled? [selection evaluation-context] (handler/adapt-every selection Integer evaluation-context))
    (run [selection] (g/with-auto-evaluation-context evaluation-context
                       (handler/adapt-every selection Integer evaluation-context))))
  (handler/defhandler :string-node-command :global
    (enabled? [selection evaluation-context] (handler/adapt-every selection StringNode evaluation-context))
    (run [selection] (g/with-auto-evaluation-context evaluation-context
                       (handler/adapt-every selection StringNode evaluation-context))))
  (handler/defhandler :other-command :global
    (enabled? [selection evaluation-context] (handler/adapt-every selection OtherType evaluation-context))
    (run [selection] (g/with-auto-evaluation-context evaluation-context
                       (handler/adapt-every selection OtherType evaluation-context))))
  (with-clean-system
    (let [[s i] (tx-nodes (g/make-nodes world
                                        [s [StringNode :string "test"]
                                         i [IntNode :int 1]]))
          selection (atom [])
          select! (fn [s] (reset! selection s))]
      (let [global (handler/->context :global {} (->DynamicSelection selection) {}
                                      {String (fn [node-id]
                                                (when (g/node-instance? StringNode node-id)
                                                  (g/node-value node-id :string)))
                                       Integer (fn [node-id]
                                                 (when (g/node-instance? IntNode node-id)
                                                   (g/node-value node-id :int)))})]
        (select! [])
        (are [enbl? cmd] (= enbl? (test-util/handler-enabled? cmd [global] {}))
             false :string-command
             false :single-string-command
             false :int-command
             false :string-node-command
             false :other-command)
        (select! [s])
        (are [enbl? cmd] (= enbl? (test-util/handler-enabled? cmd [global] {}))
             true :string-command
             true :single-string-command
             false :int-command
             true :string-node-command
             false :other-command)
        (select! [s s])
        (are [enbl? cmd] (= enbl? (test-util/handler-enabled? cmd [global] {}))
             true :string-command
             false :single-string-command
             false :int-command
             true :string-node-command
             false :other-command)
        (select! [i])
        (are [enbl? cmd] (= enbl? (test-util/handler-enabled? cmd [global] {}))
             false :string-command
             false :single-string-command
             true :int-command
             false :string-node-command
             false :other-command)))))

(deftest adapt-nested
  (handler/defhandler :string-node-command :global
    (enabled? [selection evaluation-context] (handler/adapt-every selection StringNode evaluation-context))
    (run [selection] (g/with-auto-evaluation-context evaluation-context
                       (handler/adapt-every selection StringNode evaluation-context))))
  (with-clean-system
    (let [[s] (tx-nodes (g/make-nodes world
                                      [s [StringNode :string "test"]]))
          selection (atom [])
          select! (fn [s] (reset! selection s))]
      (let [global (handler/->context :global {} (->DynamicSelection selection) {}
                              {Long :node-id})]
        (select! [])
        (is (not (test-util/handler-enabled? :string-node-command [global] {})))
        (select! [{:node-id s}])
        (is (test-util/handler-enabled? :string-node-command [global] {}))
        (select! [s])
        (is (test-util/handler-enabled? :string-node-command [global] {}))
        (select! ["other-type"])
        (is (not (test-util/handler-enabled? :string-node-command [global] {})))
        (select! [nil])
        (is (not (test-util/handler-enabled? :string-node-command [global] {})))))))

(deftest dynamics
  (handler/defhandler :string-command :global
      (active? [string] string)
      (enabled? [string] string)
      (run [string] string))
  (with-clean-system
    (let [[s] (tx-nodes (g/make-nodes world
                                      [s [StringNode :string "test"]]))]
      (let [global (handler/->context :global {:string-node s} nil {:string [:string-node :string]} {})]
        (is (test-util/handler-enabled? :string-command [global] {}))))))

(defn- eval-selection [ctxs all-selections?]
  (-> (g/with-auto-evaluation-context evaluation-context
        (handler/eval-contexts ctxs all-selections? evaluation-context))
      (first)
      (get-in [:env :selection])))

(defn- eval-selections [ctxs all-selections?]
  (mapv (fn [ctx]
          (get-in ctx [:env :selection]))
        (g/with-auto-evaluation-context evaluation-context
          (handler/eval-contexts ctxs all-selections? evaluation-context))))

(deftest contexts
  (with-clean-system
    (let [global (handler/->context :global {:selection [0]} nil {} {})]
      (is (= [0] (eval-selection [global] true))))
    (let [global (handler/->context :global {} (StaticSelection. [0]) {} {})]
      (is (= [0] (eval-selection [global] true)))
      (let [local (handler/->context :local {} (StaticSelection. [1]) {} {})]
        (is (= [[1] [1] [0]] (eval-selections [local global] true))))
      (let [local (handler/->context :local {} (StaticSelection. [1]) {} {})]
        (is (= [[1] [1]] (eval-selections [local global] false)))))))

(g/defnode ImposterStringNode
  (input source g/NodeID)
  (input string g/Str)
  (output selection-data g/Any (g/fnk [source] (when source {:alt source}))))

(defrecord AltSelection [selection-ref]
  handler/SelectionProvider
  (selection [_this _evaluation-context] @selection-ref)
  (succeeding-selection [_this _evaluation-context] [])
  (alt-selection [this evaluation-context]
    (let [s (handler/selection this evaluation-context)]
      (if-let [s' (handler/adapt-every s ImposterStringNode evaluation-context)]
        (into []
              (keep #(:alt (g/node-value % :selection-data evaluation-context)))
              s')
        []))))

(deftest alternative-selections
  (handler/defhandler :string-command :global
    (active? [selection evaluation-context] (handler/adapt-single selection StringNode evaluation-context))
    (run [selection]
      (g/with-auto-evaluation-context evaluation-context
        (let [string-node (handler/adapt-single selection StringNode evaluation-context)]
          (g/node-value string-node :string evaluation-context)))))
  (with-clean-system
    (let [[s i lonely-i] (tx-nodes (g/make-nodes world
                                                 [s [StringNode :string "test"]
                                                  i ImposterStringNode
                                                  lonely-i ImposterStringNode]
                                                 (g/connect s :string i :string)
                                                 (g/connect s :_node-id i :source)))
          selection (atom [])
          select! (fn [s] (reset! selection s))
          selection-provider (->AltSelection selection)
          global (handler/->context :global {} selection-provider {} {})]
      (is (not (test-util/handler-enabled? :string-command [global] {})))
      (select! [s])
      (is (test-util/handler-enabled? :string-command [global] {}))
      (select! [i])
      (is (test-util/handler-enabled? :string-command [global] {}))
      (select! [lonely-i])
      (is (not (test-util/handler-enabled? :string-command [global] {}))))))

(def main-menu-data [{:label (localization/message "command.file")
                      :id ::file
                      :children [{:label (localization/message "command.file.new")
                                  :id ::new
                                  :command :file.new}
                                 {:label (localization/message "command.file.open")
                                  :id ::open
                                  :command :file.open}]}
                     {:label (localization/message "command.edit")
                      :id ::edit
                      :children [{:label (localization/message "command.edit.undo")
                                  :icon "icons/undo.png"
                                  :command :edit.undo}
                                 {:label (localization/message "command.edit.redo")
                                  :icon "icons/redo.png"
                                  :command :edit.redo}]}
                     {:label (localization/message "command.help")
                      :children [{:label (localization/message "command.app.about")
                                  :command :app.about}]}])

(def scene-menu-data [{:label (localization/message "handler-test.menu.scene")
                       :children [{:label (localization/message "handler-test.menu.scene.do-stuff")}
                                  {:label :separator
                                   :id ::scene-end}]}])

(def tile-map-data [{:label (localization/message "handler-test.menu.tile-map")
                     :children [{:label (localization/message "handler-test.menu.tile-map.erase-tile")}]}])

(deftest main-menu
  (with-redefs [handler/state-atom (atom handler/empty-state)]
    (handler/register-menu! ::menubar main-menu-data)
    (handler/register-menu! ::edit scene-menu-data)
    (handler/register-menu! ::scene-end tile-map-data)
    (let [m (handler/realize-menu ::menubar)]
      (is (some? (get-in m [2 :children]))))))
