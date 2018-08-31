(ns integration.hot-reload-test
  (:require [clojure.test :refer :all]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.hot-reload :as hot-reload]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [editor.progress :as progress]
            [editor.protobuf :as protobuf])
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
        build-results (project/build! project resource-node evaluation-context nil old-artifact-map progress/null-render-progress!)]
    (when-not (contains? build-results :error)
      (workspace/artifact-map! workspace (:artifact-map build-results))
      (workspace/etags! workspace (:etags build-results)))
    build-results))

(deftest build-endpoint-test
  (test-util/with-loaded-project project-path
    (let [game-project (test-util/resource-node project "/game.project")]
      (project-build project game-project (g/make-evaluation-context))
      (let [res  (handler-get (partial hot-reload/build-handler workspace project) (->build-url "/main/main.collectionc") nil "GET")
            data (protobuf/bytes->map GameObject$CollectionDesc (->bytes (:body res)))]
        (is (= 200 (:status res)))
        (is (= "parallax" (:name data)))
        (is (= 14 (count (:instances data)))))
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
