(ns util.http-server
  (:import [java.io IOException OutputStream ByteArrayOutputStream]
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
        body-bytes (if (string? body)
                     (.getBytes ^String (:body response "") "UTF-8")
                     body)
        length     (if body
                     (count body)
                     -1)
        headers    (.getResponseHeaders e)]
    (doseq [[key value] (:headers response {})]
      (.add headers key value))
    (.sendResponseHeaders e code length)
    (when body
      (let [out (.getResponseBody e)]
        (.write out ^bytes body-bytes)
        (.close out)))
    (.close e)))

(defn ->server [port handlers]
  (let [server (HttpServer/create (InetSocketAddress. port) 0)]
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
