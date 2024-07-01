;; Copyright 2020-2024 The Defold Foundation
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

(ns integration.hot-reload-test
  (:require [clojure.test :refer :all]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.build :as build]
            [editor.defold-project :as project]
            [editor.hot-reload :as hot-reload]
            [editor.progress :as progress]
            [editor.protobuf :as protobuf]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util])
  (:import java.net.URL
           java.nio.charset.Charset
           java.io.ByteArrayInputStream
           org.apache.commons.io.IOUtils
           [com.dynamo.gameobject.proto GameObject GameObject$CollectionDesc]))

(def ^:const project-path "test/resources/build_project/SideScroller")

(defn- ->bytes [input-stream]
  (when input-stream
    (IOUtils/toByteArray input-stream)))

(defn- ->build-url [file]
  (format "%s%s" hot-reload/url-prefix file))

(defn- handler-get [handler-f url body method]
  (let [res (handler-f {:url     url
                        :headers {}
                        :method  method
                        :body    (when body (.getBytes body))})]
    (-> res
        (update :body (fn [body]
                        (let [body (if (string? body)
                                     (.getBytes body)
                                     body)]
                          (when body (ByteArrayInputStream. body)))))
        (assoc :status (:code res)))))

(defn- project-build [project resource-node evaluation-context]
  (let [workspace (project/workspace project)
        old-artifact-map (workspace/artifact-map workspace)
        build-results (build/build-project! project resource-node evaluation-context nil old-artifact-map progress/null-render-progress!)]
    (when-not (contains? build-results :error)
      (workspace/artifact-map! workspace (:artifact-map build-results))
      (workspace/etags! workspace (:etags build-results)))
    build-results))

(deftest build-endpoint-test
  (test-util/with-loaded-project project-path
    (let [game-project (test-util/resource-node project "/game.project")]
      (project-build project game-project (g/make-evaluation-context))
      (let [res  (handler-get (partial hot-reload/build-handler workspace project) (->build-url "/main/main.collectionc") nil "GET")
            data (protobuf/bytes->map-with-defaults GameObject$CollectionDesc (->bytes (:body res)))]
        (is (= 200 (:status res)))
        (is (= "parallax" (:name data)))
        (is (= 13 (count (:instances data)))))
      (is (= 404 (:status (handler-get (partial hot-reload/build-handler workspace project) (->build-url "foobar") nil "GET")))))))

(deftest etags-endpoint-test
  (test-util/with-loaded-project project-path
    (let [game-project (test-util/resource-node project "/game.project")]
      (project-build project game-project (g/make-evaluation-context))
      (let [etags (workspace/etags workspace)
            body (string/join "\n" (map (fn [[path etag]] (format "%s %s" (->build-url path) etag)) etags))
            res  (handler-get (partial hot-reload/verify-etags-handler workspace project) hot-reload/verify-etags-url-prefix body "POST")
            paths (string/split-lines (slurp (:body res)))]
        (is (> (count paths) 0))
        (is (= (count paths) (count (keys etags))))))))
