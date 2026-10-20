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

(ns editor.defold-project-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.placeholder-resource :as placeholder-resource]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]
            [util.coll :as coll]
            [util.eduction :as e]))

(g/defnode ANode
  (inherits resource-node/ResourceNode)
  (property value-piece g/Str)
  (property value g/Str
            (set (fn [evaluation-context self _old-value _new-value]
                   (let [input (g/node-value self :value-input evaluation-context)]
                     (g/set-property self :value-piece (str (first input)))))))
  (property source-resource resource/Resource
            (set (fn [evaluation-context self _old-value new-value]
                   (let [project (project/get-project (:basis evaluation-context))]
                     (:tx-data (project/connect-resource-node evaluation-context project new-value self [[:value :value-input]]))))))
  (input project-node-id g/NodeID)
  (input value-input g/Str))

(g/defnode BNode
  (inherits resource-node/ResourceNode)
  (property value g/Str))

(defn- register-resource-types [workspace types]
  (for [type types]
    (apply workspace/register-resource-type workspace (flatten (vec type)))))

(deftest loading
  (with-clean-system
    (test-util/with-ui-run-later-rebound
      (let [load-counter (atom 0)
            seen-read-opts (atom [])
            seen-load-opts (atom [])
            sanitized-owner-resources (atom [])

            read-a
            (fn read-a [read-opts _owner-resource readable]
              (swap! seen-read-opts conj read-opts)
              (read-string (slurp readable)))

            sanitize-a
            (fn sanitize-a [read-opts owner-resource source-value]
              (is (resource/resource? owner-resource))
              (is (coll/any? #(identical? read-opts %) @seen-read-opts))
              (swap! sanitized-owner-resources conj owner-resource)
              (update source-value :b #((:resolve-proj-path-fn read-opts) owner-resource %)))

            dependencies-a
            (fn dependencies-a [_read-opts _owner-resource source-value]
              (keep source-value [:b]))

            connect-a
            (fn connect-a [project self _resource]
              (g/connect project :_node-id self :project-node-id))

            load-a
            (fn load-a [{:keys [project] :as load-opts} {:keys [owner-resource resource source-value] self :node-id :as node-load-info}]
              (is (identical? owner-resource resource))
              (swap! load-counter inc)
              (swap! seen-load-opts conj load-opts)
              (is (string? (:disk-sha256 node-load-info)))
              (is (= [(:b source-value)] (:dependency-proj-paths node-load-info)))
              (let [source-resource (workspace/resolve-resource owner-resource (:b source-value))]
                (e/concat
                  (g/callback-ec
                    (fn check-connect-fn-happened [evaluation-context]
                      (is (identical? load-opts (:load-opts @(:tx-data-context evaluation-context))))
                      (is (= project (g/node-value self :project-node-id evaluation-context)))))
                  (g/set-property self :value-piece "set incorrectly")
                  (g/set-property self :source-resource source-resource)
                  (g/set-property self :value "bogus value"))))

            load-b
            (fn load-b [load-opts {:keys [owner-resource resource] self :node-id :as node-load-info}]
              (is (identical? owner-resource resource))
              (swap! load-counter inc)
              (swap! seen-load-opts conj load-opts)
              (is (not (contains? node-load-info :source-value)))
              (let [data (read-string (slurp resource))]
                (g/set-property self :value (:value data))))

            workspace (workspace/make-workspace (.getAbsolutePath (io/file "test/resources/load_project"))
                                                {}
                                                {}
                                                test-util/localization)]
        (g/transact
          (concat
            (placeholder-resource/register-resource-types workspace)
            (register-resource-types workspace [{:ext "type_a"
                                                 :node-type ANode
                                                 :read-fn (fn [read-opts owner-resource readable]
                                                            (sanitize-a read-opts owner-resource (read-a read-opts owner-resource readable)))
                                                 :dependencies-fn dependencies-a
                                                 :connect-fn connect-a
                                                 :load-fn load-a
                                                 :label "Type A"}
                                                {:ext "type_b"
                                                 :node-type BNode
                                                 :load-fn load-b
                                                 :label "Type B"}])))
        (workspace/resource-sync! workspace)
        (let [project (test-util/setup-project! workspace)
              a1 (project/get-resource-node project "/a1.type_a")]
          (is (= 3 @load-counter))
          (is (= 2 (count @seen-read-opts)))
          (is (= #{"/a1.type_a" "/a2.type_a"}
                 (into #{} (map resource/proj-path) @sanitized-owner-resources)))
          (is (coll/every? #(identical? (first @seen-read-opts) %) @seen-read-opts))
          (is (coll/every? #(ifn? (:resolve-proj-path-fn %)) @seen-read-opts))
          (is (coll/every? #(= project (:project %)) @seen-load-opts))
          (is (coll/every? #(identical? (first @seen-load-opts) %) @seen-load-opts))
          (let [expected-workspace workspace

                {:keys [code-preprocessor
                        editable->type-ext->resource-type
                        resolve-resource-fn
                        script-intelligence
                        workspace]}
                (first @seen-load-opts)

                owner-resource (workspace/find-resource workspace "/a1.type_a")
                target-resource (workspace/find-resource workspace "/a2.type_a")]

            (is (= expected-workspace workspace))
            (is (= (project/code-preprocessors project) code-preprocessor))
            (is (= (project/script-intelligence project) script-intelligence))
            (doseq [editable [true false]]
              (is (= (workspace/get-resource-type-map workspace editable)
                     (editable->type-ext->resource-type editable))))
            (with-redefs [g/now (fn [] (throw (AssertionError. "Unexpected graph query")))
                          g/unsafe-basis (fn [] (throw (AssertionError. "Unexpected graph query")))
                          g/node-value (fn [& _] (throw (AssertionError. "Unexpected graph evaluation")))
                          g/raw-property-value (fn [& _] (throw (AssertionError. "Unexpected graph property read")))]
              (is (identical? target-resource (resolve-resource-fn owner-resource "a2.type_a")))
              (is (identical? target-resource (resolve-resource-fn owner-resource "/a2.type_a")))
              (is (identical? target-resource (resolve-resource-fn nil "/a2.type_a")))
              (is (thrown-with-msg?
                    Exception
                    #"Unable to resolve relative path \"a2.type_a\" without a base-proj-path."
                    (resolve-resource-fn nil "a2.type_a")))
              (is (nil? (resolve-resource-fn owner-resource nil)))
              (is (nil? (resolve-resource-fn owner-resource "")))
              (let [missing-resource (resolve-resource-fn owner-resource "missing.type_a")]
                (is (resource/file-resource? missing-resource))
                (is (= "/missing.type_a" (resource/proj-path missing-resource)))
                (is (identical? missing-resource (resolve-resource-fn owner-resource "missing.type_a"))))))
          (is (= "t" (g/node-value a1 :value-piece))))))))

(deftest embedded-load-owner-resource
  (doseq [editability [:editable :non-editable]]
    (testing (name editability)
      (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
        (with-open [_ (test-util/make-directory-deleter project-path)]
          (when (= :non-editable editability)
            (test-util/set-non-editable-directories! project-path ["/nested"]))
          (with-clean-system
            (test-util/with-ui-run-later-rebound
              (let [workspace (test-util/setup-workspace! project-path)
                    loaded (atom [])
                    prototype-desc {:embedded-components
                                    [{:id "probe"
                                      :type "ownerprobe"
                                      :data {:path "target.type_b"}}]}]
                (g/transact
                  (workspace/register-resource-type workspace
                    :ext "ownerprobe"
                    :editable (= :editable editability)
                    :node-type BNode
                    :read-fn (fn [_read-opts _owner-resource readable]
                               (read-string (slurp readable)))
                    :write-fn pr-str
                    :load-fn (fn [load-opts {:keys [owner-resource resource source-value] :as node-load-info}]
                               (swap! loaded conj [load-opts node-load-info])
                               (is (resource/memory-resource? resource))
                               (is (= source-value (:data resource)))
                               (is (= "/nested/target.type_b"
                                      (resource/proj-path (workspace/resolve-resource owner-resource (:path source-value)))))
                               nil)))
                (test-util/write-file-resource! workspace "/nested/standalone.go" prototype-desc)
                (test-util/write-file-resource! workspace "/nested/container.collection"
                  {:name "container"
                   :embedded-instances [{:id "embedded" :data prototype-desc}]})
                (workspace/resource-sync! workspace)
                (let [project (test-util/setup-project! workspace)]
                  (is (= 2 (count @loaded)))
                  (is (= #{"/nested/standalone.go" "/nested/container.collection"}
                         (into #{} (map (comp resource/proj-path :owner-resource second)) @loaded)))
                  (is (coll/every? #(= project (:project (first %))) @loaded))
                  (is (coll/every? #(identical? (ffirst @loaded) (first %)) @loaded)))))))))))
