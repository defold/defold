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
  (let [code (:code response 200)
        body (.getBytes ^String (:body response "") "UTF-8")
        length (count body)
        headers (.getResponseHeaders e)]
    (doseq [[key value] (:headers response {})]
      (.add headers key value))
    (.sendResponseHeaders e code length)
    (with-open [out (.getResponseBody e)]
      (.write out body))))

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
