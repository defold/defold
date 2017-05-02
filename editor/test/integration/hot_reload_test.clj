(ns integration.hot-reload-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.hot-reload :as hot-reload]
            [editor.defold-project :as project]
            [integration.test-util :as test-util]
            [editor.protobuf :as protobuf]
            [support.test-support :as support])
  (:import java.net.URL
           java.nio.charset.Charset
           java.io.ByteArrayInputStream
           org.apache.commons.io.IOUtils
           [com.dynamo.gameobject.proto GameObject GameObject$CollectionDesc]))

(def ^:const project-path "test/resources/build_project/SideScroller")

(defn- ->bytes [input-stream]
  (when input-stream
    (IOUtils/toByteArray input-stream)))

(defn- handler-get [handler-f file]
  (let [res (handler-f {:url     (format "%s%s" hot-reload/url-prefix file)
                        :headers {}
                        :method  "GET"
                        :body    nil})]
    (-> res
        (update :body #(when % (ByteArrayInputStream. %)))
        (assoc :status (:code res)))))

(deftest build-endpoint-test
  (support/with-clean-system
    (let [workspace (test-util/setup-workspace! world project-path)
          project   (test-util/setup-project! workspace)
          game-project (test-util/resource-node project "/game.project")]
      (project/build-and-write project game-project {})
      (let [res  (handler-get (partial hot-reload/build-handler workspace) "/main/main.collectionc")
            data (protobuf/bytes->map GameObject$CollectionDesc (->bytes (:body res)))]
        (is (= 200 (:status res)))
        (is (= "parallax" (:name data)))
        (is (= 14 (count (:instances data)))))

      (is (= 404 (:status (handler-get (partial hot-reload/build-handler workspace) "foobar")))))))
