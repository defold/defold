(ns util.http-server
  (:require [clojure.java.io :as io]
            [service.log :as log])
  (:import [java.io IOException OutputStream ByteArrayInputStream ByteArrayOutputStream BufferedOutputStream]
           [java.net InetSocketAddress]
           [org.apache.commons.io IOUtils]
           [com.sun.net.httpserver HttpExchange HttpHandler HttpServer]))

(set! *warn-on-reflection* true)

(defn- exchange->request! [^HttpExchange e]
  {:headers (.getRequestHeaders e)
   :method (.getRequestMethod e)
   :body (let [os (ByteArrayOutputStream.)]
           (IOUtils/copy (.getRequestBody e) os)
           (.toByteArray os))
   :url (.toString (.getRequestURI e))})

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
      (let [^bytes bytes (if (string? body) (.getBytes ^String body "UTF-8") body)]
        (with-open [out (.getResponseBody e)]
          (.write out bytes))))
    (.close e)))

(defn ->server [port handlers]
  (let [server (HttpServer/create (InetSocketAddress. port) 0)
        handlers (if-let [default (get handlers "/")]
                   handlers
                   (assoc handlers "/" (fn [request]
                                         (log/info :msg (format "No handler for '%s'" (:url request)))
                                         {:code 404})))]
    (doseq [[path handler] handlers]
      (.createContext server path (reify HttpHandler
                                    (handle [this t]
                                      (try
                                        (-> (handler (exchange->request! t))
                                          (response->exchange! t))
                                        (catch Throwable t
                                          (prn (Throwable->map t))))))))
    (.setExecutor server nil)
    server))

(defn start! [^HttpServer server]
  (.start server)
  server)

(defn stop! [^HttpServer server]
  (.stop server 2)
  server)

(defn local-url [^HttpServer server]
  (format "http://localhost:%d" (.getPort (.getAddress server))))

(defn port [^HttpServer server]
  (.getPort (.getAddress server)))
