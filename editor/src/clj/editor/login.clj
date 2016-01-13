(ns editor.login
  (:require [clojure.java.io :as io]
            [editor.client :as client]
            [editor.prefs :as prefs]
            [editor.ui :as ui]
            [service.log :as log])
  (:import [com.dynamo.cr.protocol.proto Protocol$TokenExchangeInfo Protocol$TokenExchangeInfo$Type Protocol$UserInfo]
           [fi.iki.elonen NanoHTTPD NanoHTTPD$Response NanoHTTPD$IHTTPSession]
           [javafx.fxml FXMLLoader]
           [javafx.scene Parent Scene]
           [javafx.scene.web WebView]
           [javafx.stage Stage Modality]))

(set! *warn-on-reflection* true)

(defn- logged-in? [prefs client]
  (when-let [email (prefs/get-prefs prefs "email" nil)]
    (try
      (client/rget client (format "/users/%s" email) Protocol$UserInfo)
      true
      (catch Exception e
        (log/warn :exception e)))))

(defn- parse-url [url]
  (if-let [[_ token action] (re-find #"/(.+?)/(.+?)" url)]
    [token action]
    (throw (Exception. (format "invalid url %s" url)))))

(defn- exchange-info [client token]
  (client/rget client (format "/login/oauth/exchange/%s" token) Protocol$TokenExchangeInfo))

(defn- handle-request [prefs client ^NanoHTTPD$IHTTPSession session]
  (let [[token action] (parse-url (.getUri session))
         exchange-info (exchange-info client token)]
    (if (= (:type exchange-info) :SIGNUP)
      (throw (Exception. "This account is not associated with defold.com yet. Please go to defold.com to signup"))
      exchange-info)))

(defn- open-login-dialog [prefs client]
  (let [root ^Parent (ui/load-fxml "login.fxml")
        stage (Stage.)
        scene (Scene. root)
        web-view ^WebView (.lookup root "#web")
        engine (.getEngine web-view)
        return (atom false)
        server (proxy [NanoHTTPD] [0]
                 (serve [^NanoHTTPD$IHTTPSession session]
                   (try
                     (let [exchange-info (handle-request prefs client session)]
                       (prefs/set-prefs prefs "email" (:email exchange-info))
                       (prefs/set-prefs prefs "token" (:auth-token exchange-info)))
                     (reset! return true)
                     (catch Exception e
                       (reset! return e)
                       (log/error :exception e)))
                   (ui/run-later (ui/close! stage))
                   (NanoHTTPD$Response. "")))]
    (.initModality stage Modality/APPLICATION_MODAL)
    (.setTitle stage "Login")
    (.start server)
    (.setOnHidden stage (ui/event-handler event (.stop server)))
    (.load engine (format "http://cr.defold.com/login/oauth/google?redirect_to=http://localhost:%d/{token}/{action}" (.getListeningPort server)))
    (.setScene stage scene)
    (.showAndWait stage)
    (if (instance? Throwable @return)
      (throw @return)
      @return)))

(defn login [prefs]
  (let [client (client/make-client prefs)]
    (if (logged-in? prefs client)
      true
      (ui/run-now (open-login-dialog prefs client)))))

(defn logout [prefs]
  (prefs/set-prefs prefs "email" nil)
  (prefs/set-prefs prefs "token" nil))

