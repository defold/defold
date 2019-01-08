(ns util.http-server
  (:require [clojure.java.io :as io]
            [editor.error-reporting :as error-reporting]
            [service.log :as log])
  (:import [java.io InputStream IOException OutputStream ByteArrayInputStream ByteArrayOutputStream BufferedOutputStream]
           [java.net InetSocketAddress InetAddress URLDecoder]
           [org.apache.commons.io IOUtils]
           [com.sun.net.httpserver HttpExchange HttpHandler HttpServer]))

(set! *warn-on-reflection* true)

(def ^:const default-handlers {"/" (fn [request]
                                     (log/info :msg (format "No handler for '%s'" (:url request)))
                                     {:code 404})})

(defn- exchange->request! [^HttpExchange e]
  {:headers (.getRequestHeaders e)
   :method (.getRequestMethod e)
   :body (let [os (ByteArrayOutputStream.)]
           (IOUtils/copy (.getRequestBody e) os)
           (.toByteArray os))
   :url (URLDecoder/decode (.toString (.getRequestURI e)))})

(defn- response->exchange! [response ^HttpExchange e]
  (let [code       (:code response 200)
        body       (:body response)
        headers    (.getResponseHeaders e)
        in-headers (:headers response {})
        length     (if (nil? body)
                     -1
                     (if-let [content-length (get in-headers "Content-Length")]
                       (Long/parseLong content-length)
                       0))]
    (doseq [[key value] in-headers]
      (.add headers key value))
    (.sendResponseHeaders e code length)
    (when body
      (with-open [^InputStream in (io/input-stream (if (string? body) (.getBytes ^String body "UTF-8") body))
                  out (.getResponseBody e)]
        (IOUtils/copy in out)))
    (.close e)))

(defn- setup-server!
  [^HttpServer server handlers]
  (doseq [[path handler] (merge default-handlers handlers)]
    (.createContext server path (reify HttpHandler
                                  (handle [this t]
                                    (try
                                      (-> (handler (exchange->request! t))
                                          (response->exchange! t))
                                      (catch Throwable t
                                        (error-reporting/report-exception! t)))))))
  (.setExecutor server nil)
  server)

(defn ->server
  ([port handlers]
   (-> (HttpServer/create (InetSocketAddress. port) 0)
       (setup-server! handlers)))
  ([address port handlers]
   (let [inet-socket-address
         (cond
           (instance? String address)
           (InetSocketAddress. ^String address ^int port)

           (instance? InetAddress address)
           (InetSocketAddress. ^InetAddress address ^int port))]
     (-> (HttpServer/create inet-socket-address 0)
         (setup-server! handlers)))))

(defn start! [^HttpServer server]
  (.start server)
  server)

(defn stop! [^HttpServer server]
  (.stop server 2)
  server)

(defn local-url [^HttpServer server]
  (format "http://localhost:%d" (.getPort (.getAddress server))))

(defn url [^HttpServer server]
  (let [address (.getAddress server)]
    (format "http://%s:%d" (.getHostString address) (.getPort address))))

(defn port [^HttpServer server]
  (.getPort (.getAddress server)))
