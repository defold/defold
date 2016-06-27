(ns integration.hot-reload-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.hot-reload :as hotload]
            [editor.defold-project :as project]
            [integration.test-util :as test-util]
            [editor.protobuf :as protobuf]
            [support.test-support :as support]
            [util.http-server :as http])
  (:import java.net.URL
           java.nio.charset.Charset
           java.io.ByteArrayInputStream
           org.apache.commons.io.IOUtils
           [com.dynamo.gameobject.proto GameObject GameObject$CollectionDesc]))

(def ^:dynamic *port*)
(def ^:dynamic *workspace*)
(def ^:dynamic *project*)

(def ^:const project-path "resources/build_project/SideScroller")

(defn- with-http-server [f]
  (let [server (http/start! (http/->server 0 {project/hot-reload-url-prefix (hotload/build-handler *project*)}))]
    (binding [*port* (.getPort (.getAddress server))]
      (f))
    (http/stop! server)))

(defn- with-clean-project [f]
  (support/with-clean-system
    (let [workspace (test-util/setup-workspace! world project-path)
          project   (test-util/setup-project! workspace)]
      (binding [*workspace* workspace
                *project*   project]
        (f)))))

(use-fixtures :once with-clean-project with-http-server)

(defn- http-get [file]
  (let [url  (URL. (format "http://localhost:%d%s%s" *port* project/hot-reload-url-prefix file))
        conn (doto (.openConnection url) .connect .disconnect)]
    {:status  (.getResponseCode conn)
     :headers (.getHeaderFields conn)
     :body    (.getInputStream conn)}))

(defn- ->str [input-stream]
  (when input-stream
    (IOUtils/toString input-stream (Charset/forName "UTF-8"))))

(defn- ->bytes [input-stream]
  (when input-stream
    (IOUtils/toByteArray input-stream)))

(defn- handler-get [handler-f file]
  (let [res (handler-f {:url     (format "%s%s" project/hot-reload-url-prefix file)
                        :headers {}
                        :method  "GET"
                        :body    nil})]
    (-> res
        (update :body #(when % (ByteArrayInputStream. %)))
        (assoc :status (:code res)))))

(deftest build-endpoint-test
  (let [res  (handler-get (hotload/build-handler *project*) "/main/main.collectionc")
        data (protobuf/bytes->map GameObject$CollectionDesc (->bytes (:body res)))]
    (is (= 200 (:status res)))
    (is (= "parallax" (:name data)))
    (is (= 9 (count (:instances data)))))

  (is (= 404 (:status (handler-get (hotload/build-handler *project*) "foobar")))))
