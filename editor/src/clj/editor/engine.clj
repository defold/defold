(ns editor.engine
  (:require [editor
             [dialogs :as dialogs]
             [protobuf :as protobuf]
             [resource :as resource]
             [ui :as ui]
             [targets :as targets]]
            [util.http-server :as http-server])
  (:import [java.net HttpURLConnection URL]))

(set! *warn-on-reflection* true)

(def ^:const timeout 2000)

(defn- get-connection [^URL url]
  (doto ^HttpURLConnection (.openConnection url)
    (.setRequestProperty "Connection" "close")
    (.setConnectTimeout timeout)
    (.setReadTimeout timeout)
    (.setDoOutput true)
    (.setRequestMethod "POST")))

(defn reload-resource [target resource]
  (let [url  (URL. (str target "/post/@resource/reload"))
        conn ^HttpURLConnection (get-connection url)]
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
        (.disconnect conn)))))

(defn reboot [target webserver hot-reload-url-prefix]
  (let [url  (URL. (str target "/post/@system/reboot"))
        conn ^HttpURLConnection (get-connection url)]
    (try
      (let [os  (.getOutputStream conn)
            url (format "http://%s:%s%s" (targets/current-ip) (http-server/port webserver) hot-reload-url-prefix)]
        (.write os ^bytes (protobuf/map->bytes
                           com.dynamo.engine.proto.Engine$Reboot
                           {:arg1 (str "--config=resource.uri=" url)
                            :arg2 (str url "/game.projectc")}))
        (.close os))
      (let [is (.getInputStream conn)]
        (while (not= -1 (.read is))
          (Thread/sleep 10))
        (.close is)
        (.disconnect conn)
        :ok)
      (catch Exception e
        (.disconnect conn)
        false))))
