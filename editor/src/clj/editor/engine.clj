(ns editor.engine
  (:require [editor
             [dialogs :as dialogs]
             [protobuf :as protobuf]
             [resource :as resource]
             [console :as console]
             [ui :as ui]
             [targets :as targets]])
  (:import [java.net HttpURLConnection URL]
           [java.io File InputStream]
           [java.lang Process ProcessBuilder]
           [com.defold.editor Platform]))

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

(defn reboot [target local-url]
  (let [url  (URL. (str target "/post/@system/reboot"))
        conn ^HttpURLConnection (get-connection url)]
    (try
      (let [os  (.getOutputStream conn)]
        (.write os ^bytes (protobuf/map->bytes
                           com.dynamo.engine.proto.Engine$Reboot
                           {:arg1 (str "--config=resource.uri=" local-url)
                            :arg2 (str local-url "/game.projectc")}))
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

(defn- pump-output [^InputStream stdout]
  (let [buf (byte-array 1024)]
    (loop []
      (let [n (.read stdout buf)]
        (when (> n -1)
          (let [msg (String. buf 0 n)]
            (console/append-console-message! msg)
            (recur)))))))

(defn launch [launch-dir]
  (let [suffix (.getExeSuffix (Platform/getHostPlatform))
        path   (format "%s/bin/dmengine%s" (System/getProperty "defold.unpack.path") suffix)
        pb     (doto (ProcessBuilder. ^java.util.List (list path))
                 (.redirectErrorStream true)
                 (.directory launch-dir))]
    (let [p  (.start pb)
          is (.getInputStream p)]
      (.start (Thread. (fn [] (pump-output is)))))))
