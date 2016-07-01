(ns editor.hot-reload
  (:require [editor
             [defold-project :as project]
             [dialogs :as dialogs]
             [protobuf :as protobuf]
             [resource :as resource]
             [ui :as ui]])
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
    (if-let [c (get resources node-name)]
      {:code    200
       :headers {"Content-Type" "application/octet-stream"}
       :body    c}
      {:code 404}))))

(defn post-reload-resource [prefs resource]
  (future
    (let [target (:url (project/get-selected-target prefs))
          url    (URL. (str target "/post/@resource/reload"))
          conn   (doto ^HttpURLConnection (.openConnection url)
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
        (catch Exception e
          (ui/run-later (dialogs/make-alert-dialog (str "Error connecting to engine on " target))))
        (finally
          (.disconnect conn))))))
