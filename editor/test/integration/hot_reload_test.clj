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
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.build :as build]
            [editor.defold-project :as project]
            [editor.hot-reload :as hot-reload]
            [editor.protobuf :as protobuf]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [util.coll :as coll]
            [util.digest :as digest]
            [util.http-client :as http]
            [util.http-server :as http-server]
            [util.http-server.types :as http-server.types])
  (:import [com.dynamo.gameobject.proto GameObject$CollectionDesc]
           [java.io ByteArrayInputStream ByteArrayOutputStream IOException RandomAccessFile]
           [java.net URLDecoder]
           [java.nio.charset StandardCharsets]))

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

;; The index must report one coherent build snapshot, including custom resource
;; sizes, while preserving spaces, literal percent escapes and other URL data.
(deftest resource-index-test
  (test-util/with-loaded-project project-path
    (let [artifacts {"/space file.bin" {:size 8 :etag "space"}
                     "/space%20file.bin" {:size 12 :etag "percent"}
                     "/a+b.bin" {:size 0 :etag "empty"}
                     "/café.bin" {:size 1073741824 :etag "large"}}]
      (workspace/artifact-map! workspace artifacts)
      (with-open [server (http-server/start! (http-server/router-handler (hot-reload/routes workspace)))]
        (let [response @(http/request (str (http-server/url server) "/__resource_index__") :as :string)
              [header & records] (string/split-lines (:body response))]
          (is (= 200 (:status response)))
          (is (= "DM_RESOURCE_INDEX 1" header))
          (is (string/ends-with? (:body response) "\n"))
          (is (= (update-keys artifacts #(str "/build" %))
                 (coll/into-> records {}
                   (map (fn [line]
                          (let [[path size etag] (string/split line #"\t")]
                            [(URLDecoder/decode path StandardCharsets/UTF_8)
                             {:size (parse-long size) :etag etag}])))))))))))

;; Batch framing preserves binary bytes, empty files and distinct escaped
;; filenames, and rejects paths or versions outside the current build snapshot.
(deftest resource-batch-stream-test
  (test-util/with-temp-project-content {}
    (let [files [["/space file.bin" "/build/space%20file.bin" (byte-array [0 10 13 127])]
                 ["/space%20file.bin" "/build/space%2520file.bin" (byte-array [1 2 3])]
                 ["/empty.bin" "/build/empty.bin" (byte-array 0)]]
          artifacts (coll/into-> files {}
                      (map (fn [[path _ data]]
                             [path {:size (alength ^bytes data) :etag (digest/sha1-hex data)}])))
          empty-etag (:etag (artifacts "/empty.bin"))]
      (doseq [[path _ data] files]
        (let [file (workspace/build-path workspace path)]
          (io/make-parents file)
          (with-open [out (io/output-stream file)]
            (.write out ^bytes data))))
      (workspace/artifact-map! workspace artifacts)
      (with-open [server (http-server/start! (http-server/router-handler (hot-reload/routes workspace)))]
        (let [url (str (http-server/url server) "/__resource_batch__")
              request (coll/join-to-string
                        ""
                        (eduction
                          (map (fn [[path encoded _]] (str encoded \tab (:etag (artifacts path)) \newline)))
                          files))
              response @(http/request url :method "POST" :body request :as :byte-array)]
          (is (= 200 (:status response)))
          (with-open [stream (ByteArrayInputStream. (:body response))]
            (is (= "DM_RESOURCE_BATCH 1\n" (String. (.readNBytes stream 20) StandardCharsets/UTF_8)))
            (doseq [[path encoded data] files]
              (let [header (str encoded \tab (alength ^bytes data) \tab (:etag (artifacts path)) \newline)]
                (is (= header (String. (.readNBytes stream (count header)) StandardCharsets/UTF_8)))
                (is (= (vec data) (vec (.readNBytes stream (alength ^bytes data)))))))
            (is (= -1 (.read stream))))
          (doseq [[request expected] [["/build/empty.bin\told\n" 409]
                                      [(str "/build/missing.bin\t" empty-etag \newline) 404]
                                      [(str "/build/../outside.bin\t" empty-etag \newline) 404]
                                      [(str "/other/empty.bin\t" empty-etag \newline) 404]
                                      [(str "/build/bad%path\t" empty-etag \newline) 400]
                                      ["/build/empty.bin\n" 400]]]
            (is (= expected (:status @(http/request url :method "POST" :body request)))))
          (spit (workspace/build-path workspace "/space file.bin") "changed")
          (is (= 409 (:status @(http/request url
                                            :method "POST"
                                            :body (str "/build/space%20file.bin\t" (:etag (artifacts "/space file.bin")) \newline))))))))))

;; Changed or deleted files must not emit an invalid batch entry, and completed
;; entries must survive even when their bytes are still buffered.
(deftest resource-batch-changed-file-test
  (test-util/with-temp-project-content {}
    (doseq [completed-size [0 8 65535 65536 65537]]
      (let [completed-data (byte-array completed-size)
            original-data (byte-array [0 1 2 3 4 5 6 7])
            files [["/completed.bin" completed-data]
                   ["/changed.bin" original-data]
                   ["/following.bin" (byte-array [8 9])]]
            artifacts (coll/into-> files {}
                        (map (fn [[path data]]
                               [path {:size (alength ^bytes data) :etag (digest/sha1-hex data)}])))
            request (coll/join-to-string
                      ""
                      (eduction
                        (map (fn [[path _]] (str "/build" path \tab (:etag (artifacts path)) \newline)))
                        files))
            completed-header (str "DM_RESOURCE_BATCH 1\n/build/completed.bin\t"
                                  (alength completed-data) \tab (:etag (artifacts "/completed.bin")) \newline)
            changed-file (workspace/build-path workspace "/changed.bin")
            handler (get-in (hot-reload/routes workspace) ["/__resource_batch__" "POST"])]
        (doseq [[path data] files]
          (let [file (workspace/build-path workspace path)]
            (io/make-parents file)
            (with-open [out (io/output-stream file)]
              (.write out ^bytes data))))
        (workspace/artifact-map! workspace artifacts)
        (doseq [[label changed-data] [["shrinking" (byte-array [0 1 2])]
                                     ["growing" (byte-array [0 1 2 3 4 5 6 7 8])]
                                     ["rewriting without resizing" (byte-array [7 6 5 4 3 2 1 0])]
                                     ["deleting" nil]]]
          (testing (str label " with completed file size " completed-size)
            (with-open [out (io/output-stream changed-file)]
              (.write out original-data))
            (with-open [body (ByteArrayInputStream. (.getBytes request StandardCharsets/UTF_8))
                        out (ByteArrayOutputStream.)]
              (let [response (handler {:body body})]
                (is (= 200 (:status response)))
                ;; Change the file at the response/stream boundary,
                ;; without sleeps.
                (if-not changed-data
                  (io/delete-file changed-file)
                  (with-open [file-out (io/output-stream changed-file)]
                    (.write file-out ^bytes changed-data)))
                (is (thrown? IOException (http-server.types/connection-write! (:body response) out)))
                (is (= (+ (count completed-header) (alength completed-data)) (.size out)))
                (with-open [stream (ByteArrayInputStream. (.toByteArray out))]
                  (is (= completed-header (String. (.readNBytes stream (count completed-header)) StandardCharsets/UTF_8)))
                  (is (= (:etag (artifacts "/completed.bin"))
                         (digest/sha1-hex (.readNBytes stream (alength completed-data)))))
                  (is (= -1 (.read stream))))))))))))

;; Enforce the 1 MiB per-file and 4 MiB payload boundaries independently of
;; client behavior, with bounded request metadata even for empty resources.
(deftest resource-batch-limits-test
  (test-util/with-temp-project-content {}
    (let [file-size (* 1024 1024)
          etag (digest/sha1-hex (byte-array file-size))
          artifacts {"/limit.bin" {:size file-size :etag etag}
                     "/too-big.bin" {:size (inc file-size) :etag (digest/sha1-hex (byte-array (inc file-size)))}}]
      (doseq [[path {:keys [size]}] artifacts]
        (let [file (workspace/build-path workspace path)]
          (io/make-parents file)
          (with-open [out (RandomAccessFile. file "rw")]
            (.setLength out size))))
      (workspace/artifact-map! workspace artifacts)
      (with-open [server (http-server/start! (http-server/router-handler (hot-reload/routes workspace)))]
        (let [url (str (http-server/url server) "/__resource_batch__")
              record (str "/build/limit.bin\t" etag \newline)
              response @(http/request url :method "POST" :body (.repeat record 4) :as :byte-array)
              header (str "/build/limit.bin\t1048576\t" etag \newline)]
          (is (= 200 (:status response)))
          (is (= (+ 20 (* 4 (+ file-size (count header)))) (alength ^bytes (:body response))))
          (is (= 413 (:status @(http/request url :method "POST" :body (.repeat record 5)))))
          (is (= 413 (:status @(http/request url
                                            :method "POST"
                                            :body (str "/build/too-big.bin\t" (:etag (artifacts "/too-big.bin")) \newline)))))
          (is (= 400 (:status @(http/request url :method "POST" :body (.repeat record 4097)))))
          (is (= 413 (:status @(http/request url :method "POST" :body (String. (char-array (inc file-size) \x)))))))))))
