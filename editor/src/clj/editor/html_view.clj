(ns editor.html-view
  (:require
   [clojure.core.protocols :as p]
   [clojure.java.io :as io]
   [clojure.string :as string]
   [dynamo.graph :as g]
   [editor.defold-project :as project]
   [editor.dialogs :as dialogs]
   [editor.handler :as handler]
   [editor.resource :as resource]
   [editor.ui :as ui]
   [editor.view :as view]
   [service.log :as log]
   [util.http-server :as http-server]
   [editor.workspace :as workspace])
  (:import
   (java.net URI URLDecoder)
   (javafx.scene Node Parent)
   (javafx.scene.layout GridPane Priority ColumnConstraints)
   (javafx.scene.web WebEngine WebView WebEvent)
   (javafx.concurrent Worker Worker$State)
   (org.w3c.dom Document Element NodeList)
   (org.w3c.dom.events Event EventListener EventTarget)))

(set! *warn-on-reflection* true)

;; TODO: stop on closing, restart with new project id on new project.
(defonce http-server (atom nil))

(def ^:private known-content-types
  {"jpg"  "image/jpeg"
   "jpeg" "image/jpeg"
   "png"  "image/png"
   "js"   "application/javascript"
   "css"  "text/css"})

(defn- resource-handler [project request]
  (if-let [node (project/get-resource-node project (:url request))]
    (let [resource (g/node-value node :resource)
          resource-type (resource/resource-type resource)
          resource-ext (resource/ext resource)
          view-types (map :id (:view-types resource-type))]
      (cond
        (some #{:html} view-types)
        {:code    200
         :headers {"Content-Type" "text/html; charset: utf-8"}
         :body    (g/node-value node :html)}

        (contains? known-content-types resource-ext)
        {:code    200
         :headers {"Content-Type" (known-content-types resource-ext)}
         :body    resource}

        :else
        (do (log/warn :message (format "Unknown content-type %s for %s" resource-ext resource))
            {:code 404})))
    (do (log/warn :message (format "Cannot find resource for %s" (:url request)))
        {:code 404})))

(defn- init-http-server! [project]
  (when-not @http-server
    (let [http-handlers {"/", (partial resource-handler project)}
          server (http-server/->server 0 http-handlers)]
      (http-server/start! server)
      (reset! http-server server)))
  @http-server)

;; make NodeList reducible

(extend-protocol p/CollReduce
  NodeList
  (coll-reduce
    ([coll f]
     (let [n (.getLength coll)]
       (if (zero? n)
         (f)
         (loop [ret nil
                i 0]
           (if (< i n)
             (let [ret (f ret (.item coll i))]
               (if (reduced? ret)
                 @ret
                 (recur ret (inc i))))
             ret)))))
    ([coll f val]
     (let [n (.getLength coll)]
       (loop [ret val
              i 0]
         (if (< i n)
           (let [ret (f ret (.item coll i))]
             (if (reduced? ret)
               @ret
               (recur ret (inc i))))
           ret))))))

;;--------------------------------------------------------------------
;; view

(defn- query-params->map
  [params]
  (when (not (string/blank? params))
    (into {}
          (map (fn [param]
                 (let [[k v] (string/split param #"=")]
                   [(keyword k) (URLDecoder/decode v "UTF-8")])))
          (string/split params #"&"))))

(defmulti url->command (fn [^URI uri dispatch-args] (.getHost uri)))

(defmethod url->command "open"
  [^URI uri {:keys [project]}]
  (let [params (query-params->map (.getQuery uri))
        proj-path (:path params)
        resource-id (project/get-resource-node project proj-path)
        resource (g/node-value resource-id :resource)]
    {:command   :open
     :user-data {:resources [resource]}}))

(defmethod url->command :default
  [^URI uri _]
  {:command (keyword (.getHost uri))})

(defn- dispatch-url!
  [project ^URI uri]
  (when-some [{:keys [command user-data]} (url->command uri {:project project})]
    (when-let [handler-ctx (handler/active command (ui/contexts (.getScene (ui/main-stage))) user-data)]
      (when (handler/enabled? handler-ctx)
        (handler/run handler-ctx)))))

(defn- string->url
  ^URI [^String str]
  (when (some? str)
    (try
      (URI. str)
      (catch java.net.URISyntaxException _
        nil))))

(defn- anchor-url
  ^URI [^Element anchor-element]
  (some-> anchor-element (.getAttribute "href") string->url))

(defn- handle-defold-click!
  [project ^Event ev]
  (when-some [url (anchor-url (.getTarget ev))]
    (dispatch-url! project url)))

(defn- hijack-defold-links
  [^Document doc project]
  (let [handler (reify EventListener
                  (handleEvent [_this ev]
                    (.preventDefault ev)
                    (handle-defold-click! project ev)))]
    (run! (fn [^Element anchor-element]
            (when-some [url (anchor-url anchor-element)]
              (when (= "defold" (.getScheme url))
                (.addEventListener ^EventTarget anchor-element "click" handler true))))
          (.getElementsByTagName doc "a"))))

(defn- open-external-ahrefs-in-browser
  [^Document doc]
  (let [handler (reify EventListener
                  (handleEvent [_this ev]
                    (.preventDefault ev)
                    (ui/open-url (anchor-url (.getTarget ev)))))]
    (run! (fn [^Element anchor-element]
            (when-some [url (anchor-url anchor-element)]
              (when (and (not= "defold" (.getScheme url))
                         (some? (.getHost url)))
                (.addEventListener ^EventTarget anchor-element "click"  handler true))))
          (.getElementsByTagName doc "a"))))

(defn- on-load-succeeded
  [^WebEngine web-engine project]
  (when-some [doc (.getDocument web-engine)]
    (doto doc
      (hijack-defold-links project)
      (open-external-ahrefs-in-browser))))

(defn- on-load-failed
  [^WebEngine web-engine]
  (let [load-worker (.getLoadWorker web-engine)]
    (when-some [ex (.getException load-worker)]
      (dialogs/make-alert-dialog (str (.getMessage load-worker)
                                      ": "
                                      (.getMessage ex))))))

(g/defnode WebViewNode
  (inherits view/WorkbenchView)
  (property web-view WebView))

(defn- ^WebView make-web-view
  [project]
  (let [web-view (WebView.)
        web-engine (.getEngine web-view)
        load-worker (.getLoadWorker web-engine)]
    (ui/observe (.stateProperty load-worker)
                (fn [_ old new]
                  (condp = new
                    Worker$State/SUCCEEDED (on-load-succeeded web-engine project)
                    Worker$State/FAILED (on-load-failed web-engine)
                    nil)))
    web-view))

(defn- resource-url [http-server resource]
  (when resource
    (str (http-server/local-url http-server)
         (resource/proj-path resource))))

(defn- load-resource! [web-engine resource]
  (let [url         (resource-url @http-server resource)
        current-url (.getLocation web-engine)]
    (when (not= url current-url)
      (.load web-engine url))))

(defn- update-web-view! [view-id]
  (let [web-view ^WebView (g/node-value view-id :web-view)
        web-engine (.getEngine web-view)]
    (when-let [resource-node (g/node-value view-id :resource-node)]
      (load-resource! web-engine (g/node-value resource-node :resource)))))

(defn- handle-location-change! [project view-id new-location]
  (if-let [new-resource-node
           (if (string/starts-with? new-location (http-server/local-url @http-server))
             (let [resource-path (subs new-location (count (http-server/local-url @http-server)))]
               (project/get-resource-node project resource-path))
             nil)]
    (g/transact (view/connect-resource-node view-id new-resource-node))
    (log/warn :message (format "Moving to non-local url or missing resource: %s" new-location))))

(defn make-view
  [graph ^Parent parent html-node opts]
  (let [project       (or (:project opts) (project/get-project html-node))
        _             (init-http-server! project)
        web-view      (make-web-view project)
        web-engine    (.getEngine web-view)
        view-id       (g/make-node! graph WebViewNode :web-view web-view)
        repainter     (ui/->timer 1 "update-web-view!" (fn [_ _] (update-web-view! view-id)))]

    (.addListener (.locationProperty web-engine)
                  (ui/change-listener _ _ new-location (handle-location-change! project view-id new-location)))

    (ui/children! parent [web-view])
    (ui/fill-control web-view)

    (ui/context! web-view :browser {:web-engine web-engine} nil)

    (doto web-engine
      (.setOnAlert (ui/event-handler ev (dialogs/make-alert-dialog (.getData ^WebEvent ev))))
      (.setUserStyleSheetLocation (str (io/resource "markdown.css")))
      (load-resource! (g/node-value html-node :resource)))

    (ui/timer-start! repainter)
    (when-some [^Tab tab (:tab opts)]
      (ui/timer-stop-on-closed! tab repainter))
    view-id))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :html
                                :label "Web"
                                :make-view-fn make-view))

(handler/defhandler :reload :browser
  (run [^WebEngine web-engine]
    (.reload web-engine)))

(ui/extend-menu ::menubar :editor.app-view/edit-end
                [{:command :reload :label "Reload"}])
