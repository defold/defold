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
           (javafx.beans.value ChangeListener)
           (javafx.scene Parent Scene)
           (javafx.scene.control ButtonBase Label TextField)
           (javafx.scene.input Clipboard ClipboardContent)))

(set! *warn-on-reflection* true)

(defonce ^:private last-used-email-prefs-key "login-last-used-email")
(defonce ^:private create-account-url "https://www.defold.com/account/signup/")

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
            (if-let [[token _action] (parse-url (.getUri session))]
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

(defn- set-prefs-from-successful-login! [prefs {:keys [auth-token email first-name last-name user-id]}]
  (assert (string? auth-token))
  (assert (string? email))
  (assert (string? first-name))
  (assert (string? last-name))
  (assert (integer? user-id))
  (prefs/set-prefs prefs "email" email)
  (prefs/set-prefs prefs "first-name" first-name)
  (prefs/set-prefs prefs "last-name" last-name)
  (prefs/set-prefs prefs "token" auth-token)
  (prefs/set-prefs prefs "user-id" user-id))

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
  (assert (case sign-in-state (:not-signed-in :browser-open :login-fields :login-fields-submitted :signed-in :forgot-password :forgot-password-submitted :email-sent) true false))
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

(defn abort-incomplete-sign-in!
  "Returns true if the user is signed in. Otherwise, restores the dashboard
  client to the initial state, then return false."
  [{:keys [sign-in-state-property] :as _dashboard-client}]
  (if (= :signed-in (sign-in-state sign-in-state-property))
    true
    (do
      (set-sign-in-state! sign-in-state-property :not-signed-in)
      false)))

(defn- connection-error-value [{:keys [reason type]}]
  (assert (= :connection-error type))
  (error-values/error-fatal (str reason "\nPlease ensure you are connected to the Internet")))

(defn- bad-response-error-value [{:keys [code reason type]}]
  (assert (= :bad-response type))
  (error-values/error-fatal (str "Communication Error: " code " " reason "\nPlease ensure a firewall is not interfering with the connection")))

(defn- begin-sign-in-with-email!
  "Sign in with E-mail and password on a background thread. The request is
  cancelled if the user navigates away from the login fields. If successful,
  stores the login token in prefs and sets the sign-in-state-property to signal
  a successful login. If bad credentials are provided, returns to the login
  fields and sets the error-message-property to notify the user."
  [{:keys [prefs sign-in-state-property] :as _dashboard-client} ^SimpleObjectProperty network-error-property ^SimpleObjectProperty account-error-property ^String email ^String password]
  (assert (= :login-fields (sign-in-state sign-in-state-property)))
  (.set network-error-property nil)
  (.set account-error-property nil)
  (set-sign-in-state! sign-in-state-property :login-fields-submitted)

  ;; Disregard delayed response if the user navigated away from the login fields.
  (let [navigated-away-atom (atom false)]
    (ui/observe-once sign-in-state-property (fn [_ _ _] (reset! navigated-away-atom true)))
    (future
      (try
        (with-open [client (client/make-client prefs)]
          (let [result (client/login-with-email client email password)]
            (ui/run-later
              ;; User might have left the login form, in which case we ignore the response.
              ;; In effect, this cancels the login, since we do not store the login token.
              (when-not @navigated-away-atom
                (if (= :success (:type result))
                  (let [login-info (:login-info result)]
                    (prefs/set-prefs prefs last-used-email-prefs-key email)
                    (set-prefs-from-successful-login! prefs login-info)
                    (set-sign-in-state! sign-in-state-property :signed-in))
                  (do
                    (set-sign-in-state! sign-in-state-property :login-fields)
                    (case (:type result)
                      :connection-error
                      (.set network-error-property (connection-error-value result))

                      :bad-response
                      (.set network-error-property (bad-response-error-value result))

                      :unauthorized
                      (.set account-error-property (error-values/error-fatal "Wrong E-mail address or Password")))))))))
        (catch Throwable error
          (error-reporting/report-exception! error)
          (ui/run-later
            (set-sign-in-state! sign-in-state-property :login-fields)))))))

(defn- begin-reset-password!
  [{:keys [prefs sign-in-state-property] :as _dashboard-client} ^SimpleObjectProperty network-error-property ^SimpleObjectProperty account-error-property ^String email]
  (assert (= :forgot-password (sign-in-state sign-in-state-property)))
  (.set network-error-property nil)
  (.set account-error-property nil)
  (set-sign-in-state! sign-in-state-property :forgot-password-submitted)
  (let [navigated-away-atom (atom false)]
    (future
      (try
        (with-open [client (client/make-client prefs)]
          (let [result (client/forgot-password client email)]
            (ui/run-later
              ;; The user might have navigated away before we get a response back.
              ;; In case the password reset was successful, we still want to
              ;; inform the user that it happened.
              (if (= :success (:type result))
                (set-sign-in-state! sign-in-state-property :email-sent)
                (when-not @navigated-away-atom
                  (set-sign-in-state! sign-in-state-property :forgot-password)
                  (case (:type result)
                    :connection-error
                    (.set network-error-property (connection-error-value result))

                    :bad-response
                    (.set network-error-property (bad-response-error-value result))

                    :not-found ; There is no Defold account for the specified E-mail address.
                    (.set account-error-property (error-values/error-fatal "This address does not have a Defold account"))))))))
        (catch Throwable error
          (error-reporting/report-exception! error)
          (ui/run-later
            (set-sign-in-state! sign-in-state-property :forgot-password)))))))

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
  [{:keys [sign-in-response-server-atom sign-in-state-property] :as dashboard-client}]
  (assert (= :not-signed-in (sign-in-state sign-in-state-property)))
  (let [sign-in-response-server (start-sign-in-response-server! dashboard-client)
        sign-in-page-url (login-page-url sign-in-response-server)]
    (set-sign-in-state! sign-in-state-property :browser-open)
    (ui/open-url sign-in-page-url)

    ;; As we leave the screen, terminate the sign-in response server if it has
    ;; not yet been shut down gracefully.
    (ui/observe-once sign-in-state-property
                     (fn [_ _ _]
                       (when-some [sign-in-response-server (preset! sign-in-response-server-atom nil)]
                         (stop-server-now! sign-in-response-server))))))

(defn- cancel-sign-in!
  "Cancel the sign-in process and return to the :not-signed-in screen."
  [{:keys [sign-in-state-property] :as _dashboard-client}]
  (assert (case (sign-in-state sign-in-state-property) (:browser-open :login-fields :login-fields-submitted) true false))
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
    (error-values/error-info "Required")

    (not (.find (re-matcher email-regex email)))
    (error-values/error-fatal "Must be a valid E-mail address")))

(defn- password-error-value [^String password]
  (when (string/blank? password)
    (error-values/error-info "Required")))

(defn- severity-style-class [severity]
  (case severity
    nil nil
    :fatal "error"
    :warning "warning"
    :info "info"))

(def ^:private error-value-style-class (comp severity-style-class :severity))

(defn- configure-validated-field! [^TextField field ^Label error-label error-observable]
  (let [error-style (b/map error-value-style-class error-observable)]
    (b/bind! (.textProperty error-label) (b/map :message error-observable))
    (b/bind-style! error-label :error-severity error-style)
    (b/bind-style! field :error-severity (b/when-not (.focusedProperty field)
                                           error-style))))

(defn- configure-state-login-fields! [state-login-fields {:keys [prefs sign-in-state-property] :as dashboard-client}]
  (let [email-validation-error-property (SimpleObjectProperty.)
        password-validation-error-property (SimpleObjectProperty.)
        network-error-property (SimpleObjectProperty.)
        account-error-property (SimpleObjectProperty.)]
    (ui/bind-presence! state-login-fields
                       (b/or (b/= :login-fields sign-in-state-property)
                             (b/= :login-fields-submitted sign-in-state-property)))
    (ui/with-controls state-login-fields [cancel-button create-account-button email-error-label ^TextField email-field ^Label error-banner-label forgot-password-button password-error-label ^TextField password-field ^ButtonBase submit-button]
      (let [on-submit! (fn [_]
                         (when (ui/enabled? submit-button)
                           (begin-sign-in-with-email! dashboard-client network-error-property account-error-property (.getText email-field) (.getText password-field))))]
        (ui/on-action! password-field on-submit!)
        (ui/on-action! submit-button on-submit!)
        (ui/on-action! cancel-button (fn [_] (cancel-sign-in! dashboard-client)))
        (ui/on-action! forgot-password-button (fn [_] (set-sign-in-state! sign-in-state-property :forgot-password)))
        (ui/on-action! create-account-button (fn [_] (ui/open-url create-account-url))))

      ;; Display network errors as a banner.
      (ui/bind-presence! error-banner-label (b/some? network-error-property))
      (b/bind! (.textProperty error-banner-label) (b/map :message network-error-property))

      ;; Populate email field from prefs.
      (ui/text! email-field (prefs/get-prefs prefs last-used-email-prefs-key ""))

      ;; Clear password field when entering the login fields state, then move
      ;; focus to the relevant input field.
      (ui/observe sign-in-state-property
                  (fn [_ old-sign-in-state new-sign-in-state]
                    (when (and (= :login-fields new-sign-in-state)
                               (not= :login-fields-submitted old-sign-in-state))
                      (ui/text! password-field "")
                      (let [focused-field (if (some? (.getValue email-validation-error-property))
                                            email-field
                                            password-field)]
                        (ui/request-focus! focused-field)))))

      ;; Clear account error when any of the fields are edited.
      (ui/observe (.textProperty email-field) (fn [_ _ _] (.set account-error-property nil)))
      (ui/observe (.textProperty password-field) (fn [_ _ _] (.set account-error-property nil)))

      ;; Configure email field.
      (b/bind! email-validation-error-property (b/map email-error-value (.textProperty email-field)))
      (configure-validated-field! email-field email-error-label email-validation-error-property)

      ;; Configure password field.
      (b/bind! password-validation-error-property (b/map password-error-value (.textProperty password-field)))
      (configure-validated-field! password-field password-error-label (b/if (b/some? account-error-property)
                                                                        account-error-property
                                                                        password-validation-error-property))

      ;; When submitted, disable the submit button until we get a response.
      ;; We also disable the submit button if the form contains errors.
      (b/bind! (.disableProperty submit-button)
               (b/or (b/= :login-fields-submitted sign-in-state-property)
                     (b/some? email-validation-error-property)
                     (b/some? password-validation-error-property))))))

(defn- configure-state-not-signed-in! [state-not-signed-in {:keys [sign-in-state-property] :as dashboard-client}]
  (ui/bind-presence! state-not-signed-in (b/= :not-signed-in sign-in-state-property))
  (ui/with-controls state-not-signed-in [create-account-button sign-in-with-browser-button sign-in-with-email-button]
    (ui/on-action! sign-in-with-browser-button (fn [_] (begin-sign-in-with-browser! dashboard-client)))
    (ui/on-action! sign-in-with-email-button (fn [_] (set-sign-in-state! sign-in-state-property :login-fields)))
    (ui/on-action! create-account-button (fn [_] (ui/open-url create-account-url)))))

(defn- configure-state-browser-open! [state-browser-open {:keys [sign-in-state-property] :as dashboard-client}]
  (ui/bind-presence! state-browser-open (b/= :browser-open sign-in-state-property))
  (ui/with-controls state-browser-open [cancel-button copy-sign-in-url-button]
    (ui/on-action! cancel-button (fn [_] (cancel-sign-in! dashboard-client)))
    (ui/on-action! copy-sign-in-url-button (fn [_] (copy-sign-in-url! dashboard-client (Clipboard/getSystemClipboard))))))

(defn- configure-state-forgot-password! [state-forgot-password {:keys [sign-in-state-property] :as dashboard-client}]
  (let [email-validation-error-property (SimpleObjectProperty.)
        network-error-property (SimpleObjectProperty.)
        account-error-property (SimpleObjectProperty.)]
    (ui/bind-presence! state-forgot-password (b/or (b/= :forgot-password sign-in-state-property)
                                                   (b/= :forgot-password-submitted sign-in-state-property)))
    (ui/with-controls state-forgot-password [cancel-button ^TextField email-field email-error-label ^Label error-banner-label ^ButtonBase submit-button]
      (let [on-submit! (fn [_]
                         (when (ui/enabled? submit-button)
                           (begin-reset-password! dashboard-client network-error-property account-error-property (.getText email-field))))]
        (ui/on-action! email-field on-submit!)
        (ui/on-action! submit-button on-submit!)
        (ui/on-action! cancel-button (fn [_] (set-sign-in-state! sign-in-state-property :login-fields))))

      ;; Display network errors as a banner.
      (ui/bind-presence! error-banner-label (b/some? network-error-property))
      (b/bind! (.textProperty error-banner-label) (b/map :message network-error-property))

      ;; Move focus to the email field when entering the forgot password state.
      (ui/observe sign-in-state-property
                  (fn [_ old-sign-in-state new-sign-in-state]
                    (when (and (= :forgot-password new-sign-in-state)
                               (not= :forgot-password-submitted old-sign-in-state))
                      (ui/request-focus! email-field))))

      ;; Clear account error when any of the fields are edited.
      (ui/observe (.textProperty email-field) (fn [_ _ _] (.set account-error-property nil)))

      ;; Configure email field.
      (b/bind! email-validation-error-property (b/map email-error-value (.textProperty email-field)))
      (configure-validated-field! email-field email-error-label (b/if (b/some? account-error-property)
                                                                  account-error-property
                                                                  email-validation-error-property))

      ;; When submitted, disable the submit button until we get a response.
      ;; We also disable the submit button if the form contains errors.
      (b/bind! (.disableProperty submit-button)
               (b/or (b/= :forgot-password-submitted sign-in-state-property)
                     (b/some? email-validation-error-property))))))

(defn- configure-state-email-sent! [state-email-sent {:keys [sign-in-state-property] :as _dashboard-client}]
  (ui/bind-presence! state-email-sent (b/= :email-sent sign-in-state-property))
  (ui/with-controls state-email-sent [cancel-button]
    (ui/on-action! cancel-button (fn [_] (set-sign-in-state! sign-in-state-property :login-fields)))))

(defn configure-sign-in-ui-elements! [root dashboard-client]
  (assert (dashboard-client? dashboard-client))
  (ui/with-controls root [^Parent state-login-fields state-not-signed-in state-browser-open ^Parent state-forgot-password state-email-sent]
    (configure-state-login-fields! state-login-fields dashboard-client)
    (configure-state-not-signed-in! state-not-signed-in dashboard-client)
    (configure-state-browser-open! state-browser-open dashboard-client)
    (configure-state-forgot-password! state-forgot-password dashboard-client)
    (configure-state-email-sent! state-email-sent dashboard-client)
    (let [^TextField state-login-fields-email-field (.lookup state-login-fields "#email-field")
          ^TextField state-forgot-password-email-field (.lookup state-forgot-password "#email-field")]
      (b/bind-bidirectional! (.textProperty state-forgot-password-email-field)
                             (.textProperty state-login-fields-email-field)))))

(declare sign-out!)

(defn configure-sign-out-button! [sign-out-button {:keys [sign-in-state-property] :as dashboard-client}]
  (assert (dashboard-client? dashboard-client))
  (ui/bind-presence! sign-out-button (b/= :signed-in sign-in-state-property))
  (ui/on-action! sign-out-button (fn [_] (sign-out! dashboard-client))))

(defn- show-sign-in-dialog! [{:keys [^SimpleObjectProperty sign-in-state-property] :as dashboard-client}]
  (let [root (doto (ui/load-fxml "login/login-dialog.fxml")
               (configure-sign-in-ui-elements! dashboard-client))
        scene (Scene. root)
        stage (doto (ui/make-dialog-stage)
                (ui/title! "Sign In")
                (.setScene scene))
        sign-in-state-listener (reify ChangeListener
                                 (changed [_ _ _ sign-in-state]
                                   (when (= :signed-in sign-in-state)
                                     (ui/close! stage))))]
    (.addListener sign-in-state-property sign-in-state-listener)
    (ui/show-and-wait! stage)
    (let [signed-in? (abort-incomplete-sign-in! dashboard-client)]
      (.removeListener sign-in-state-property sign-in-state-listener)
      signed-in?)))

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
  (prefs/set-prefs prefs "user-id" nil)
  (set-sign-in-state! sign-in-state-property :not-signed-in))

(defn can-sign-out? [{:keys [prefs] :as _dashboard-client}]
  (some? (prefs/get-prefs prefs "token" nil)))
