(ns editor.login
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.analytics :as analytics]
            [editor.client :as client]
            [editor.error-reporting :as error-reporting]
            [editor.prefs :as prefs]
            [editor.ui :as ui]
            [editor.ui.bindings :as b]
            [internal.graph.error-values :as error-values]
            [service.log :as log]
            [util.thread-util :refer [preset!]])
  (:import (clojure.lang Atom)
           (com.sun.jersey.api.client UniformInterfaceException)
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

(defn- logged-in? [prefs]
  (if (nil? (prefs/get-prefs prefs "email" nil))
    false
    (with-open [client (client/make-client prefs)]
      (try
        (client/user-info client)
        true
        (catch UniformInterfaceException e
          ;; A "403 Forbidden" response is normal if the user is not logged in.
          ;; No need to log it.
          (let [status-info (.getStatusInfo (.getResponse e))
                status-code (.getStatusCode status-info)]
            (when (not= 403 status-code)
              (log/warn :exception e)))
          false)
        (catch Exception e
          (log/warn :exception e)
          false)))))

(defn- make-create-account-url
  ^String [prefs]
  (if (client/using-stage-server? prefs)
    "https://stage.defold.com/account/signup"
    "https://www.defold.com/account/signup"))

(defn- make-redirect-to-url
  ^String [^NanoHTTPD server]
  (format "http://localhost:%d/action/{action}/{token}" (.getListeningPort server)))

(defn- make-sign-in-page-url
  ^String [prefs ^NanoHTTPD server]
  (str (.resolve (client/server-url prefs)
                 (str "/login/oauth/google?redirect_to="
                      (URLEncoder/encode (make-redirect-to-url server))))))

(defonce ^:private login-token-regex #"^\/action\/([^\/]+)\/([0-9a-f]+)")

(defn- parse-login-token
  ^String [^String session-uri]
  (when-some [[_ action token] (re-find login-token-regex session-uri)]
    (when (and (= "login" action) (not-empty token))
      token)))

(defonce ^:private not-found-response (NanoHTTPD$Response. NanoHTTPD$Response$Status/NOT_FOUND NanoHTTPD/MIME_PLAINTEXT (.getDescription NanoHTTPD$Response$Status/NOT_FOUND)))

(defn- html-string-response
  ^NanoHTTPD$Response [^String html]
  (NanoHTTPD$Response. html))

(def ^:private signup-required-html-template
  (slurp (io/resource "login/browser/signup-required.html")))

(defn- make-signup-required-html
  ^String [prefs]
  (string/replace signup-required-html-template
                  "{{create-account-url}}"
                  (make-create-account-url prefs)))

(def ^:private make-signup-required-response (comp html-string-response make-signup-required-html))

(def ^:private error-html-template
  (slurp (io/resource "login/browser/error.html")))

(defn- make-error-html
  ^String [^String error-message]
  (string/replace error-html-template
                  "{{error-message}}"
                  error-message))

(def ^:private make-error-response (comp html-string-response make-error-html))

(def ^:private static-resources
  {"/style.css" ["text/css"
                 "login/browser/style.css"]
   "/login-successful.html" ["text/html"
                             "login/browser/login-successful.html"]})

(defn- static-resource-response
  ^NanoHTTPD$Response [session-uri]
  (when-some [[^String mime-type path] (static-resources session-uri)]
    (NanoHTTPD$Response. NanoHTTPD$Response$Status/OK mime-type (io/input-stream (io/resource path)))))

(defn- make-server
  ^NanoHTTPD [client exchange-response-fn]
  (doto (proxy [NanoHTTPD] [0]
          (serve [^NanoHTTPD$IHTTPSession session]
            (try
              (let [session-uri (.getUri session)]
                (if-some [login-token (parse-login-token session-uri)]
                  (exchange-response-fn (client/exchange-info client login-token))
                  (or (static-resource-response session-uri)
                      not-found-response)))
              (catch Throwable error
                (exchange-response-fn {:type :exception
                                       :exception error})))))
    (.start)))

(defn- set-prefs-from-successful-login! [prefs {:keys [auth-token email first-name last-name tracking-id]}]
  (assert (string? auth-token))
  (assert (string? email))
  (assert (string? first-name))
  (assert (string? last-name))
  (assert (string? (not-empty tracking-id)))
  (prefs/set-prefs prefs "email" email)
  (prefs/set-prefs prefs "first-name" first-name)
  (prefs/set-prefs prefs "last-name" last-name)
  (prefs/set-prefs prefs "token" auth-token)
  (analytics/set-uid! tracking-id))

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

(def ^:private valid-sign-in-states
  #{:browser-open
    :email-sent
    :forgot-password
    :forgot-password-submitted
    :login-fields
    :login-fields-submitted
    :not-signed-in
    :signed-in})

(defn- sign-in-state [^SimpleObjectProperty sign-in-state-property]
  (.get sign-in-state-property))

(defn- set-sign-in-state! [^SimpleObjectProperty sign-in-state-property sign-in-state]
  (assert (contains? valid-sign-in-states sign-in-state))
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

(defn- connection-error-message
  ^String [{:keys [reason type]}]
  (assert (= :connection-error type))
  (str reason "\nPlease ensure you are connected to the Internet and that we're not blocked by a firewall"))

(def ^:private connection-error-value (comp error-values/error-fatal connection-error-message))

(defn- bad-response-error-message
  ^String [{:keys [code reason type]}]
  (assert (= :bad-response type))
  (str "Communication Error: " code " " reason "\nPlease ensure a proxy is not interfering with the connection"))

(def ^:private bad-response-error-value (comp error-values/error-fatal bad-response-error-message))

(defn- begin-sign-in-with-email!
  "Sign in with e-mail and password on a background thread. The request is
  cancelled if the user navigates away from the login fields. If successful,
  stores the login token in prefs and sets the sign-in-state-property to signal
  a successful login. If bad credentials are provided, returns to the login
  fields and sets the error-message-property to notify the user."
  [{:keys [prefs sign-in-state-property] :as _dashboard-client} ^SimpleObjectProperty network-error-property ^SimpleObjectProperty account-error-property ^String email ^String password]
  (assert (= :login-fields (sign-in-state sign-in-state-property)))
  (.set network-error-property nil)
  (.set account-error-property nil)
  (analytics/track-event! "login-and-signup" "login-submit" "email")
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
                    (analytics/track-event! "login-and-signup" "login-success" "email")
                    (set-sign-in-state! sign-in-state-property :signed-in))
                  (do
                    (analytics/track-event! "login-and-signup" "login-fail" "email")
                    (set-sign-in-state! sign-in-state-property :login-fields)
                    (case (:type result)
                      :connection-error
                      (.set network-error-property (connection-error-value result))

                      :bad-response
                      (.set network-error-property (bad-response-error-value result))

                      :unauthorized
                      (.set account-error-property (error-values/error-fatal "Wrong e-mail address or Password")))))))))
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

                    :not-found ; There is no Defold account for the specified e-mail address.
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
        exchange-response-fn (fn [result]
                               ;; Take ownership of the server. We will shut it
                               ;; down after the browser has finished. We clear
                               ;; the atom so that stop-server-now! does not
                               ;; prematurely stop the server if the user leaves
                               ;; the :browser-open state.
                               (let [sign-in-response-server (preset! sign-in-response-server-atom nil)]
                                 ;; React to the result on the main thread.
                                 (ui/run-later
                                   (client/destroy-client! client)
                                   (stop-server-soon! sign-in-response-server)
                                   (if (= :success (:type result))
                                     (let [exchange-info (:exchange-info result)]
                                       (set-prefs-from-successful-login! prefs exchange-info)
                                       (analytics/track-event! "login-and-signup" "login-success" "google")
                                       (set-sign-in-state! sign-in-state-property :signed-in))
                                     (do
                                       (analytics/track-event! "login-and-signup" "login-fail" "google")
                                       (set-sign-in-state! sign-in-state-property :not-signed-in)
                                       (when (= :exception (:type result))
                                         (error-reporting/report-exception! (:exception result))))))

                                 ;; Return a suitable HTML response to the browser.
                                 (case (:type result)
                                   :bad-response
                                   (make-error-response (bad-response-error-message result))

                                   :connection-error
                                   (make-error-response (connection-error-message result))

                                   :exception
                                   (make-error-response (.getMessage ^Throwable (:exception result)))

                                   :signup-required
                                   (make-signup-required-response prefs)

                                   :success
                                   (static-resource-response "/login-successful.html"))))]
    (swap! sign-in-response-server-atom
           (fn [old-sign-in-response-server]
             (when (some? old-sign-in-response-server)
               (stop-server-now! old-sign-in-response-server))
             (make-server client exchange-response-fn)))))

(defn- begin-sign-in-with-browser!
  "Open a sign-in page in the system-configured web browser. Start a server that
  awaits the oauth response redirect from the browser."
  [{:keys [prefs sign-in-response-server-atom sign-in-state-property] :as dashboard-client}]
  (assert (= :not-signed-in (sign-in-state sign-in-state-property)))
  (let [sign-in-response-server (start-sign-in-response-server! dashboard-client)
        sign-in-page-url (make-sign-in-page-url prefs sign-in-response-server)]
    (analytics/track-event! "login-and-signup" "login-start" "google")
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
  (let [sign-in-state (sign-in-state sign-in-state-property)]
    (assert (case sign-in-state (:browser-open :login-fields :login-fields-submitted) true false))
    (analytics/track-event! "login-and-signup" "login-cancel" (if (= :browser-open sign-in-state) "google" "email"))
    (set-sign-in-state! sign-in-state-property :not-signed-in)))

(defn- copy-sign-in-url!
  "Copy the sign-in url from the sign-in response server to the clipboard.
  If the sign-in response server is not running, does nothing."
  [{:keys [prefs sign-in-response-server-atom sign-in-state-property] :as _dashboard-client} ^Clipboard clipboard]
  (assert (= :browser-open (sign-in-state sign-in-state-property)))
  (when-some [sign-in-response-server @sign-in-response-server-atom]
    (let [sign-in-page-url (make-sign-in-page-url prefs sign-in-response-server)]
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
    (error-values/error-fatal "Must be a valid e-mail address")))

(defn- password-error-value [^String password]
  (cond
    (string/blank? password)
    (error-values/error-info "Required")

    (> 8 (count password))
    (error-values/error-fatal "Minimum eight characters")))

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

(defn- show-create-account-page! [prefs]
  (analytics/track-event! "login-and-signup" "signup-intent")
  (ui/open-url (make-create-account-url prefs)))

(defn- configure-state-login-fields! [state-login-fields {:keys [prefs sign-in-state-property] :as dashboard-client}]
  (let [email-validation-error-property (SimpleObjectProperty.)
        password-validation-error-property (SimpleObjectProperty.)
        network-error-property (SimpleObjectProperty.)
        account-error-property (SimpleObjectProperty.)]
    (b/bind-presence! state-login-fields
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
        (ui/on-action! create-account-button (fn [_] (show-create-account-page! prefs))))

      ;; Display network errors as a banner.
      (b/bind-presence! error-banner-label (b/some? network-error-property))
      (b/bind! (.textProperty error-banner-label) (b/map :message network-error-property))

      ;; Populate email field from prefs.
      (ui/text! email-field (prefs/get-prefs prefs last-used-email-prefs-key ""))

      ;; Clear password field when entering the login fields state, then move
      ;; focus to the relevant input field. We also clear errors.
      (ui/observe sign-in-state-property
                  (fn [_ old-sign-in-state new-sign-in-state]
                    (when (and (= :login-fields new-sign-in-state)
                               (not= :login-fields-submitted old-sign-in-state))
                      (.set network-error-property nil)
                      (.set account-error-property nil)
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

(defn- sign-in-motivation-text
  ^String [sign-in-intent]
  (case sign-in-intent
    :access-projects "You need to sign in to access your\nprojects in the Defold cloud."
    :fetch-libraries "You need to sign in to fetch libraries\nthat are hosted in the Defold cloud."
    :sign-ios-app    "You need to sign in to upload the\nsigned app to the Defold cloud."
    :synchronize     "You need to sign in to synchronize\nyour project with the Defold cloud."
    :upload-project  "You need to sign in to upload your\nproject to the Defold cloud."))

(defn- show-login-fields! [sign-in-state-property]
  (analytics/track-event! "login-and-signup" "login-start" "email")
  (set-sign-in-state! sign-in-state-property :login-fields))

(defn- configure-state-not-signed-in! [state-not-signed-in {:keys [prefs sign-in-state-property] :as dashboard-client} sign-in-intent]
  (b/bind-presence! state-not-signed-in (b/= :not-signed-in sign-in-state-property))
  (ui/with-controls state-not-signed-in [create-account-button sign-in-motivation-label sign-in-with-browser-button sign-in-with-email-button]
    (ui/text! sign-in-motivation-label (sign-in-motivation-text sign-in-intent))
    (ui/on-action! sign-in-with-browser-button (fn [_] (begin-sign-in-with-browser! dashboard-client)))
    (ui/on-action! sign-in-with-email-button (fn [_] (show-login-fields! sign-in-state-property)))
    (ui/on-action! create-account-button (fn [_] (show-create-account-page! prefs)))))

(defn- configure-state-browser-open! [state-browser-open {:keys [sign-in-state-property] :as dashboard-client}]
  (b/bind-presence! state-browser-open (b/= :browser-open sign-in-state-property))
  (ui/with-controls state-browser-open [cancel-button copy-sign-in-url-button]
    (ui/on-action! cancel-button (fn [_] (cancel-sign-in! dashboard-client)))
    (ui/on-action! copy-sign-in-url-button (fn [_] (copy-sign-in-url! dashboard-client (Clipboard/getSystemClipboard))))))

(defn- configure-state-forgot-password! [state-forgot-password {:keys [sign-in-state-property] :as dashboard-client}]
  (let [email-validation-error-property (SimpleObjectProperty.)
        network-error-property (SimpleObjectProperty.)
        account-error-property (SimpleObjectProperty.)]
    (b/bind-presence! state-forgot-password (b/or (b/= :forgot-password sign-in-state-property)
                                                  (b/= :forgot-password-submitted sign-in-state-property)))
    (ui/with-controls state-forgot-password [cancel-button ^TextField email-field email-error-label ^Label error-banner-label ^ButtonBase submit-button]
      (let [on-submit! (fn [_]
                         (when (ui/enabled? submit-button)
                           (begin-reset-password! dashboard-client network-error-property account-error-property (.getText email-field))))]
        (ui/on-action! email-field on-submit!)
        (ui/on-action! submit-button on-submit!)
        (ui/on-action! cancel-button (fn [_] (set-sign-in-state! sign-in-state-property :login-fields))))

      ;; Display network errors as a banner.
      (b/bind-presence! error-banner-label (b/some? network-error-property))
      (b/bind! (.textProperty error-banner-label) (b/map :message network-error-property))

      ;; Clear errors and move focus to the email field when entering the forgot
      ;; password state.
      (ui/observe sign-in-state-property
                  (fn [_ old-sign-in-state new-sign-in-state]
                    (when (and (= :forgot-password new-sign-in-state)
                               (not= :forgot-password-submitted old-sign-in-state))
                      (.set network-error-property nil)
                      (.set account-error-property nil)
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
  (b/bind-presence! state-email-sent (b/= :email-sent sign-in-state-property))
  (ui/with-controls state-email-sent [cancel-button]
    (ui/on-action! cancel-button (fn [_] (set-sign-in-state! sign-in-state-property :login-fields)))))

(defn configure-sign-in-ui-elements! [root dashboard-client sign-in-intent]
  (assert (dashboard-client? dashboard-client))
  (ui/with-controls root [^Parent state-login-fields state-not-signed-in state-browser-open ^Parent state-forgot-password state-email-sent]
    (configure-state-login-fields! state-login-fields dashboard-client)
    (configure-state-not-signed-in! state-not-signed-in dashboard-client sign-in-intent)
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
  (b/bind-presence! sign-out-button (b/= :signed-in sign-in-state-property))
  (ui/on-action! sign-out-button (fn [_] (sign-out! dashboard-client))))

(defn- show-sign-in-dialog! [{:keys [^SimpleObjectProperty sign-in-state-property] :as dashboard-client} sign-in-intent]
  (let [root (doto (ui/load-fxml "login/login-dialog.fxml")
               (configure-sign-in-ui-elements! dashboard-client sign-in-intent))
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

(defn signed-in?
  "Returns true if the dashboard client believes the user is signed in to the
  Defold dashboard. Does not actually verify that the sign in is still valid
  with the server, but should be usable directly after construction or after a
  call to sign-in!"
  [{:keys [sign-in-state-property] :as _dashboard-client}]
  (case (sign-in-state sign-in-state-property)
    :signed-in true
    false))

(defn sign-in!
  "Ensures the user is signed in to the Defold dashboard. If the user is already
  signed in, return true. If not, opens a Sign In dialog in a nested event loop.
  Blocks until the user has dismissed the dialog. Returns true if the user
  successfully signed in."
  [{:keys [prefs] :as dashboard-client} sign-in-intent]
  (or (logged-in? prefs)
      (ui/run-now
        (show-sign-in-dialog! dashboard-client sign-in-intent))))

(defn sign-out!
  "Sign out the active user from the Defold dashboard."
  [{:keys [prefs sign-in-state-property] :as _dashboard-client}]
  (analytics/track-event! "login-and-signup" "logout")
  (analytics/set-uid! nil)
  (prefs/set-prefs prefs "email" nil)
  (prefs/set-prefs prefs "first-name" nil)
  (prefs/set-prefs prefs "last-name" nil)
  (prefs/set-prefs prefs "token" nil)
  (set-sign-in-state! sign-in-state-property :not-signed-in))

(defn can-sign-out? [{:keys [prefs] :as _dashboard-client}]
  (some? (prefs/get-prefs prefs "token" nil)))
