(ns editor.hot-reload
  (:require [editor
             [defold-project :as project]
             [protobuf :as protobuf]
             [resource :as resource]])
  (:import com.dynamo.resource.proto.Resource
           [java.net HttpURLConnection URL]))

(set! *warn-on-reflection* true)

(defn build-handler [project]
  (fn [{:keys [url]}]
    (let [node-name (subs url (count project/hot-reload-url-prefix))
          node      (project/get-resource-node project "/game.project")
          res       (project/build project node {})
          resources (into {} (keep (fn [d]
                                     (when-let [r (:resource d)]
                                       [(resource/proj-path r) (:content d)]))
                                   res))]
    (println "build-handler" node-name (boolean (get resources node-name)))
    (if-let [c (get resources node-name)]
      {:code    200
       :headers {"Content-Type" "application/octet-stream"}
       :body    c}
      {:code 404}))))

(defn post-reload-resource [resource]
  (future
    (let [url  (URL. "http://localhost:8001/post/@resource/reload")
          conn (doto ^HttpURLConnection (.openConnection url)
                 (.setDoOutput true) (.setRequestMethod "POST"))]
      (try
        (let [os (.getOutputStream conn)]
          (.write os ^bytes (protobuf/map->bytes
                             com.dynamo.resource.proto.Resource$Reload
                             {:resource (str (resource/proj-path resource) "c")}))
          (.close os))
        (let [is (.getInputStream conn)]
          (while (not= -1 (.read is))
            (Thread/sleep 10))
          (.close is))
        (println "response done")
        (catch Exception e
          (println e))
        (finally
          (.disconnect conn))))))
