(ns editor.login
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.client :as client]
            [editor.error-reporting :as error-reporting]
            [editor.prefs :as prefs]
            [editor.ui :as ui]
            [editor.ui.bindings :as b]
            [internal.graph.error-values :as error-values]
            [service.log :as log]
            [util.thread-util :refer [preset!]])
  (:import (clojure.lang Atom)
           (fi.iki.elonen NanoHTTPD NanoHTTPD$IHTTPSession NanoHTTPD$Response NanoHTTPD$Response$Status)
           (java.net URLEncoder)
           (java.util.prefs Preferences)
           (javafx.beans.property SimpleObjectProperty)
           (javafx.scene Scene)
           (javafx.scene.control ButtonBase Label TextField)
           (javafx.scene.input Clipboard ClipboardContent)))

(set! *warn-on-reflection* true)

(defonce ^:private last-used-email-prefs-key "login-last-used-email")

(defn- logged-in? [prefs]
  (if (nil? (prefs/get-prefs prefs "email" nil))
    false
    (with-open [client (client/make-client prefs)]
      (try
        (client/user-info client)
        true
        (catch Exception e
          (log/warn :exception e)
          false)))))

(defn- make-redirect-to-url
  [^NanoHTTPD server]
  (format "http://localhost:%d/{token}/{action}" (.getListeningPort server)))

(defn- parse-url [url]
  (when-let [[_ token action] (re-find #"/(.+?)/(.+?)" url)]
    [token action]))

(def ^:private login-successful-html
  (slurp (io/resource "login-successful.html")))

(defn- make-server
  ^NanoHTTPD [client {:keys [on-success on-error]}]
  (doto (proxy [NanoHTTPD] [0]
          (serve [^NanoHTTPD$IHTTPSession session]
            (if-let [[token action] (parse-url (.getUri session))]
              (try
                (let [exchange-info (client/exchange-info client token)]
                  (on-success exchange-info)
                  (NanoHTTPD$Response. login-successful-html))
                (catch Exception e
                  (log/error :exception e)
                  (on-error e)
                  (NanoHTTPD$Response. (format "<h1>Login failed: %s</h1>" (.getMessage e)))))
              ;; Handle other requests, for example /favicon.ico with a 404.
              (NanoHTTPD$Response. (NanoHTTPD$Response$Status/NOT_FOUND) NanoHTTPD/MIME_PLAINTEXT "Not found"))))
    (.start)))

(defn- set-prefs-from-successful-login! [prefs {:keys [auth-token email first-name last-name]}]
  (assert (string? auth-token))
  (assert (string? email))
  (assert (string? first-name))
  (assert (string? last-name))
  (prefs/set-prefs prefs "email" email)
  (prefs/set-prefs prefs "first-name" first-name)
  (prefs/set-prefs prefs "last-name" last-name)
  (prefs/set-prefs prefs "token" auth-token))

(defn login-page-url
  ^String [^NanoHTTPD server]
  (format "https://cr.defold.com/login/oauth/google?redirect_to=%s" (URLEncoder/encode (make-redirect-to-url server))))

(defn- stop-server-now! [^NanoHTTPD server]
  (.stop server))

(defn- stop-server-soon! [^NanoHTTPD server]
  ;; delay the shutdown here to give the server some time to
  ;; flush the response to the client before it is stopped.
  (ui/->future 0.5 #(.stop server)))

(defn credentials [prefs]
  (let [email (prefs/get-prefs prefs "email" nil)
        token (prefs/get-prefs prefs "token" nil)]
    (when (and email token)
      [email token])))

;; -----------------------------------------------------------------------------
;; Dashboard client
;; -----------------------------------------------------------------------------

(defn- sign-in-state [^SimpleObjectProperty sign-in-state-property]
  (.get sign-in-state-property))

(defn- set-sign-in-state! [^SimpleObjectProperty sign-in-state-property sign-in-state]
  (assert (case sign-in-state (:not-signed-in :browser-open :login-fields :signed-in) true false))
  (.set sign-in-state-property sign-in-state))

(defn dashboard-client? [value]
  (and (map? value)
       (instance? Atom (:sign-in-response-server-atom value))
       (instance? SimpleObjectProperty (:sign-in-state-property value))
       (instance? Preferences (:prefs value))))

(defn make-dashboard-client [prefs]
  (let [sign-in-state (if (logged-in? prefs) :signed-in :not-signed-in)
        sign-in-response-server-atom (atom nil)
        sign-in-state-property (doto (SimpleObjectProperty.)
                                 (set-sign-in-state! sign-in-state))]
    {:sign-in-response-server-atom sign-in-response-server-atom
     :sign-in-state-property sign-in-state-property
     :prefs prefs}))

(defn stop-sign-in-response-server! [{:keys [sign-in-response-server-atom] :as _dashboard-client}]
  (when-some [sign-in-response-server (preset! sign-in-response-server-atom nil)]
    (stop-server-now! sign-in-response-server)))

(defn- begin-sign-in-with-email!
  "Sign in with E-mail and password on a background thread. The request is
  cancelled if the user navigates away from the login fields. If successful,
  stores the login token in prefs and sets the sign-in-state-property to signal
  a successful login. If bad credentials are provided, returns to the login
  fields and sets the error-message-property to notify the user."
  [{:keys [prefs sign-in-state-property] :as _dashboard-client} ^SimpleObjectProperty login-error-property ^String email ^String password]
  (assert (= :login-fields (sign-in-state sign-in-state-property)))
  (.set login-error-property nil)
  (set-sign-in-state! sign-in-state-property :login-fields-submitted)

  ;; Disregard delayed response if the user navigated away from the login fields.
  (let [sign-in-cancelled-atom (atom false)]
    (ui/observe-once sign-in-state-property (fn [_ _ _] (reset! sign-in-cancelled-atom true)))
    (future
      (try
        (with-open [client (client/make-client prefs)]
          (let [login-info (client/login-with-email client email password)]
            (ui/run-later
              ;; User might have cancelled the login, in which case we ignore the response.
              (when-not @sign-in-cancelled-atom
                (if-some [error-message (:error-message login-info)]
                  (do
                    (.set login-error-property (error-values/error-fatal error-message))
                    (set-sign-in-state! sign-in-state-property :login-fields))
                  (do
                    (prefs/set-prefs prefs last-used-email-prefs-key email)
                    (set-prefs-from-successful-login! prefs login-info)
                    (set-sign-in-state! sign-in-state-property :signed-in)))))))
        (catch Throwable error
          (error-reporting/report-exception! error)
          (ui/run-later
            (set-sign-in-state! sign-in-state-property :login-fields)))))))

(defn- start-sign-in-response-server!
  "Starts a server that awaits an oauth response and sets the sign-in state
  depending on the response. Returns the started server instance."
  [{:keys [prefs sign-in-response-server-atom sign-in-state-property] :as _dashboard-client}]
  (let [client (client/make-client prefs)
        server-opts {:on-success (fn [exchange-info]
                                   (client/destroy-client! client)
                                   (ui/run-later
                                     (stop-server-soon! (preset! sign-in-response-server-atom nil))
                                     (set-prefs-from-successful-login! prefs exchange-info)
                                     (set-sign-in-state! sign-in-state-property :signed-in)))
                     :on-error (fn [exception]
                                 (client/destroy-client! client)
                                 (ui/run-later
                                   (stop-server-soon! (preset! sign-in-response-server-atom nil))
                                   (set-sign-in-state! sign-in-state-property :not-signed-in)
                                   (error-reporting/report-exception! exception)))}]
    (swap! sign-in-response-server-atom
           (fn [old-sign-in-response-server]
             (when (some? old-sign-in-response-server)
               (stop-server-now! old-sign-in-response-server))
             (make-server client server-opts)))))

(defn- begin-sign-in-with-browser!
  "Open a sign-in page in the system-configured web browser. Start a server that
  awaits the oauth response redirect from the browser."
  [{:keys [sign-in-state-property] :as dashboard-client}]
  (assert (= :not-signed-in (sign-in-state sign-in-state-property)))
  (let [sign-in-response-server (start-sign-in-response-server! dashboard-client)
        sign-in-page-url (login-page-url sign-in-response-server)]
    (set-sign-in-state! sign-in-state-property :browser-open)
    (ui/open-url sign-in-page-url)))

(defn- cancel-sign-in!
  "Cancel the sign-in process and return to the :not-signed-in screen."
  [{:keys [sign-in-response-server-atom sign-in-state-property] :as _dashboard-client}]
  (assert (case (sign-in-state sign-in-state-property) (:browser-open :login-fields) true false))
  (when-some [sign-in-response-server (preset! sign-in-response-server-atom nil)]
    (stop-server-now! sign-in-response-server))
  (set-sign-in-state! sign-in-state-property :not-signed-in))

(defn- copy-sign-in-url!
  "Copy the sign-in url from the sign-in response server to the clipboard.
  If the sign-in response server is not running, does nothing."
  [{:keys [sign-in-response-server-atom sign-in-state-property] :as _dashboard-client} ^Clipboard clipboard]
  (assert (= :browser-open (sign-in-state sign-in-state-property)))
  (when-some [sign-in-response-server @sign-in-response-server-atom]
    (let [sign-in-page-url (login-page-url sign-in-response-server)]
      (.setContent clipboard (doto (ClipboardContent.)
                               (.putString sign-in-page-url)
                               (.putUrl sign-in-page-url))))))

;; -----------------------------------------------------------------------------
;; UI elements
;; -----------------------------------------------------------------------------

(def ^:private email-regex #"^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$") ; Not exhaustive, but should cover 99% of emails found in the wild.

(defn- email-error-value [^String email]
  (cond
    (string/blank? email)
    (error-values/error-fatal "Required")

    (not (.find (re-matcher email-regex email)))
    (error-values/error-fatal "Must be a valid E-mail address")))

(defn- password-error-value [^String password]
  (when (string/blank? password)
    (error-values/error-fatal "Required")))

(defn- configure-state-login-fields! [state-login-fields {:keys [prefs ^SimpleObjectProperty sign-in-state-property] :as dashboard-client}]
  (let [email-validation-error-property (SimpleObjectProperty.)
        password-validation-error-property (SimpleObjectProperty.)
        login-error-property (SimpleObjectProperty.)]
    (ui/bind-presence! state-login-fields
                       (b/or (b/= :login-fields sign-in-state-property)
                             (b/= :login-fields-submitted sign-in-state-property)))
    (ui/with-controls state-login-fields [cancel-button ^Label email-error-label ^TextField email-field forgot-password-button ^Label password-error-label ^TextField password-field ^ButtonBase submit-button]
      (ui/on-action! cancel-button (fn [_] (cancel-sign-in! dashboard-client)))
      (ui/on-action! submit-button (fn [_] (begin-sign-in-with-email! dashboard-client login-error-property (.getText email-field) (.getText password-field))))
      (ui/on-action! forgot-password-button (fn [_] (ui/open-url "https://www.defold.com"))) ;; TODO: Direct link to Forgot Password page.

      ;; When clicked, disable the submit button until we get a response.
      ;; We also disable the submit button if the form contains errors.
      (b/bind! (.disableProperty submit-button)
               (b/or (b/= :login-fields-submitted sign-in-state-property)
                     (b/blank? (.textProperty email-field))
                     (b/blank? (.textProperty password-field))
                     (b/some? email-validation-error-property)
                     (b/some? password-validation-error-property)))

      ;; Email field errors.
      (let [email-error-property (b/when-not (.focusedProperty email-field)
                                   (b/if (b/some? login-error-property)
                                     login-error-property
                                     email-validation-error-property))]
        (b/bind! (.visibleProperty email-error-label) (b/some? email-error-property))
        (b/bind! (.textProperty email-error-label) (b/map email-error-property :message))
        (ui/bind-error-value-style! email-error-property [email-field email-error-label])
        (ui/on-focus! email-field (fn [got-focus?]
                                    (when-not got-focus?
                                      (.set email-validation-error-property
                                            (email-error-value (.getText email-field)))))))

      ;; Password field errors.
      (let [password-error-property (b/when-not (.focusedProperty password-field)
                                      (b/if (b/some? login-error-property)
                                        login-error-property
                                        password-validation-error-property))]
        (b/bind! (.visibleProperty password-error-label) (b/some? password-error-property))
        (b/bind! (.textProperty password-error-label) (b/map password-error-property :message))
        (ui/bind-error-value-style! password-error-property [password-field password-error-label])
        (ui/on-focus! password-field (fn [got-focus?]
                                       (when-not got-focus?
                                         (.set password-validation-error-property
                                               (password-error-value (.getText password-field)))))))

      ;; Populate fields from prefs when entering the login fields state.
      (ui/observe sign-in-state-property
                  (fn [_ old-sign-in-state new-sign-in-state]
                    (when (and (= :login-fields new-sign-in-state)
                               (not= :login-fields-submitted old-sign-in-state))
                      (ui/text! email-field (prefs/get-prefs prefs last-used-email-prefs-key ""))))))))

(defn- configure-state-not-signed-in! [state-not-signed-in {:keys [^SimpleObjectProperty sign-in-state-property] :as dashboard-client}]
  (ui/bind-presence! state-not-signed-in (b/= :not-signed-in sign-in-state-property))
  (ui/with-controls state-not-signed-in [create-account-button sign-in-with-browser-button sign-in-with-email-button]
    (ui/on-action! sign-in-with-browser-button (fn [_] (begin-sign-in-with-browser! dashboard-client)))
    (ui/on-action! sign-in-with-email-button (fn [_] (set-sign-in-state! sign-in-state-property :login-fields)))
    (ui/on-action! create-account-button (fn [_] (ui/open-url "https://www.defold.com")))))

(defn- configure-state-state-browser-open! [state-browser-open {:keys [^SimpleObjectProperty sign-in-state-property] :as dashboard-client}]
  (ui/bind-presence! state-browser-open (b/= :browser-open sign-in-state-property))
  (ui/with-controls state-browser-open [cancel-button copy-sign-in-url-button]
    (ui/on-action! cancel-button (fn [_] (cancel-sign-in! dashboard-client)))
    (ui/on-action! copy-sign-in-url-button (fn [_] (copy-sign-in-url! dashboard-client (Clipboard/getSystemClipboard))))))

(defn configure-sign-in-ui-elements! [root dashboard-client]
  (assert (dashboard-client? dashboard-client))
  (ui/with-controls root [state-login-fields state-not-signed-in state-browser-open]
    (configure-state-login-fields! state-login-fields dashboard-client)
    (configure-state-not-signed-in! state-not-signed-in dashboard-client)
    (configure-state-state-browser-open! state-browser-open dashboard-client)))

(declare sign-out!)

(defn configure-sign-out-button! [sign-out-button {:keys [^SimpleObjectProperty sign-in-state-property] :as dashboard-client}]
  (assert (dashboard-client? dashboard-client))
  (ui/bind-presence! sign-out-button (b/= :signed-in sign-in-state-property))
  (ui/on-action! sign-out-button (fn [_] (sign-out! dashboard-client))))

(defn- show-sign-in-dialog! [{:keys [sign-in-state-property] :as dashboard-client}]
  (let [root (doto (ui/load-fxml "login/login-dialog.fxml")
               (configure-sign-in-ui-elements! dashboard-client))
        scene (Scene. root)
        stage (doto (ui/make-dialog-stage)
                (ui/title! "Sign In")
                (.setScene scene))]
    (ui/show-and-wait! stage)
    (= :signed-in (sign-in-state sign-in-state-property))))

(defn sign-in!
  "Ensures the user is signed in to the Defold dashboard. If the user is already
  signed in, return true. If not, opens a Sign In dialog in a nested event loop.
  Blocks until the user has dismissed the dialog. Returns true if the user
  successfully signed in."
  [{:keys [prefs] :as dashboard-client}]
  (or (logged-in? prefs)
      (ui/run-now
        (show-sign-in-dialog! dashboard-client))))

(defn sign-out!
  "Sign out the active user from the Defold dashboard."
  [{:keys [prefs sign-in-state-property] :as _dashboard-client}]
  (prefs/set-prefs prefs "email" nil)
  (prefs/set-prefs prefs "first-name" nil)
  (prefs/set-prefs prefs "last-name" nil)
  (prefs/set-prefs prefs "token" nil)
  (set-sign-in-state! sign-in-state-property :not-signed-in))

(defn can-sign-out? [{:keys [prefs] :as _dashboard-client}]
  (some? (prefs/get-prefs prefs "token" nil)))
