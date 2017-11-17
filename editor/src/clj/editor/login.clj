(ns editor.login
  (:require [clojure.java.io :as io]
            [editor.client :as client]
            [editor.prefs :as prefs]
            [editor.ui :as ui]
            [service.log :as log])
  (:import [com.dynamo.cr.protocol.proto Protocol$TokenExchangeInfo Protocol$TokenExchangeInfo$Type Protocol$UserInfo]
           [java.net URLEncoder]
           [fi.iki.elonen NanoHTTPD NanoHTTPD$Response NanoHTTPD$Response$Status NanoHTTPD$IHTTPSession]
           [javafx.fxml FXMLLoader]
           [javafx.scene Parent Scene]
           [javafx.stage Stage Modality]))

(set! *warn-on-reflection* true)

(defn- logged-in? [prefs client]
  (if-let [email (prefs/get-prefs prefs "email" nil)]
    (try
      (client/rget client (format "/users/%s" email) Protocol$UserInfo)
      true
      (catch Exception e
        (log/warn :exception e)
        false))
    false))

(defn- make-redirect-to-url
  [^NanoHTTPD server]
  (format "http://localhost:%d/{token}/{action}" (.getListeningPort server)))

(defn- parse-url [url]
  (when-let [[_ token action] (re-find #"/(.+?)/(.+?)" url)]
    [token action]))

(defn- get-exchange-info [client token]
  (let [exchange-info (client/rget client (format "/login/oauth/exchange/%s" token) Protocol$TokenExchangeInfo)]
    (if (= (:type exchange-info) :SIGNUP)
      (throw (Exception. "This account is not associated with defold.com yet. Please go to defold.com to signup"))
      exchange-info)))

(defn- make-server
  ^NanoHTTPD [client {:keys [on-success on-error]}]
  (doto (proxy [NanoHTTPD] [0]
          (serve [^NanoHTTPD$IHTTPSession session]
            (if-let [[token action] (parse-url (.getUri session))]
              (try
                (let [exchange-info (get-exchange-info client token)]
                  (on-success exchange-info)
                  (NanoHTTPD$Response. "<p>Login successful. You can now close this browser tab and return to the defold editor.</p>"))
                (catch Exception e
                  (log/error :exception e)
                  (on-error e)
                  (NanoHTTPD$Response. (format "<h1>Login failed: %s</h1>" (.getMessage e)))))
              ;; Handle other requests, for example /favicon.ico with a 404.
              (NanoHTTPD$Response. (NanoHTTPD$Response$Status/NOT_FOUND) NanoHTTPD/MIME_PLAINTEXT "Not found"))))
    (.start)))

(defn- open-login-dialog [prefs client]
  (let [root ^Parent (ui/load-fxml "login.fxml")
        stage (ui/make-dialog-stage)
        scene (Scene. root)
        return (atom false)
        close-stage! (fn [] (ui/run-later
                              ;; delay the closing here to give the server some time to
                              ;; flush the response to the client before it is stopped.
                              (ui/->future 0.5 #(ui/close! stage))))
        server (make-server client {:on-success (fn [exchange-info]
                                                  (prefs/set-prefs prefs "email" (:email exchange-info))
                                                  (prefs/set-prefs prefs "first-name" (:first-name exchange-info))
                                                  (prefs/set-prefs prefs "last-name" (:last-name exchange-info))
                                                  (prefs/set-prefs prefs "token" (:auth-token exchange-info))
                                                  (reset! return true)
                                                  (close-stage!))
                                    :on-error   (fn [exception]
                                                  (reset! return exception)
                                                  (close-stage!))})
        redirect-to-url (make-redirect-to-url server)
        url (format "https://cr.defold.com/login/oauth/google?redirect_to=%s" (URLEncoder/encode redirect-to-url))]
    (ui/with-controls root [^Button cancel ^TextArea link]
      (ui/text! link url)
      (ui/on-action! cancel (fn [_] (.close stage))))
    (.setTitle stage "Login")
    (.setOnHidden stage (ui/event-handler event (.stop server)))
    (ui/open-url url)
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
  (prefs/set-prefs prefs "first-name" nil)
  (prefs/set-prefs prefs "last-name" nil)
  (prefs/set-prefs prefs "token" nil))

(defn has-token? [prefs]
  (some? (prefs/get-prefs prefs "token" nil)))

(defn credentials [prefs]
  (let [email (prefs/get-prefs prefs "email" nil)
        token (prefs/get-prefs prefs "token" nil)]
    (when (and email token)
      [email token])))
