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

(ns integration.hot-reload-test
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.build :as build]
            [editor.defold-project :as project]
            [editor.hot-reload :as hot-reload]
            [editor.protobuf :as protobuf]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [util.coll :as coll]
            [util.http-client :as http]
            [util.http-server :as http-server])
  (:import [com.dynamo.gameobject.proto GameObject$CollectionDesc]))

(def ^:const project-path "test/resources/build_project/SideScroller")

(defn- ->build-url [file]
  (format "%s%s" hot-reload/url-prefix file))

(defn- project-build [project resource-node evaluation-context]
  (let [workspace (project/workspace project)
        old-artifact-map (workspace/artifact-map workspace)
        build-results (build/build-project! project resource-node old-artifact-map nil evaluation-context)]
    (when-not (contains? build-results :error)
      (workspace/artifact-map! workspace (:artifact-map build-results))
      (workspace/etags! workspace (:etags build-results)))
    build-results))

(deftest build-endpoint-test
  (test-util/with-loaded-project project-path
    (with-open [server (http-server/start! (http-server/router-handler (hot-reload/routes workspace)))]
      (let [game-project (test-util/resource-node project "/game.project")]
        (project-build project game-project (g/make-evaluation-context))
        (let [res  @(http/request (str (http-server/url server) (->build-url "/main/main.collectionc")) :as :byte-array)
              data (protobuf/bytes->map-with-defaults GameObject$CollectionDesc (:body res))]
          (is (= 200 (:status res)))
          (is (= "parallax" (:name data)))
          (is (= 13 (count (:instances data)))))
        (is (= 404 (:status @(http/request (str (http-server/url server) (->build-url "foobar"))))))))))

(deftest etags-endpoint-test
  (test-util/with-loaded-project project-path
    (with-open [server (http-server/start! (http-server/router-handler (hot-reload/routes workspace)))]
      (let [game-project (test-util/resource-node project "/game.project")]
        (project-build project game-project (g/make-evaluation-context))
        (let [etags (workspace/etags workspace)
              body (string/join "\n" (map (fn [[path etag]] (format "%s %s" (->build-url path) etag)) etags))
              res @(http/request (str (http-server/url server) hot-reload/verify-etags-url-prefix) :method "POST" :body body :as :string)
              paths (string/split-lines (:body res))]
          (is (> (count paths) 0))
          (is (= (count paths) (count (keys etags)))))))))

;; Batch validation must echo exact cache keys and reject missing or stale tags,
;; so the engine cannot trust another file or lose encoded URL characters.
(deftest etag-cache-keys-test
  (test-util/with-loaded-project project-path
    (workspace/etags! workspace {"/folder/a b.bin" "tag-a"
                                 "/folder/a%20b.bin" "tag-percent"
                                 "/other.bin" "tag-b"})
    (let [keys ["http://localhost:8000/build/folder/a%20b.bin"
                "https://localhost:8443/build/other.bin"
                "/build/other.bin"
                "http://localhost:8000/build/folder/a%2520b.bin"]
          request (coll/join-to-string "\n"
                                       [(str (keys 0) " tag-a")
                                        (str (keys 1) " tag-b")
                                        (str (keys 2) " tag-b")
                                        (str (keys 3) " tag-percent")
                                        "http://localhost:8000/build/other.bin stale"
                                        "http://localhost:8000/build/missing.bin"
                                        "http://localhost:8000/build/missing.bin "
                                        "http://localhost:8000/build-other/other.bin tag-b"
                                        "http://localhost:8000/build/../other.bin tag-b"
                                        "invalid%url tag-b"])]
      (is (= keys (#'hot-reload/body->valid-entries workspace request))))))
