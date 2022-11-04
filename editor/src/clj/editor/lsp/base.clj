(ns editor.lsp.base
  (:require [clojure.core.async :as a]
            [clojure.string :as string]
            [editor.error-reporting :as error-reporting]
            [editor.lsp.async :as lsp.async])
  (:import [java.io InputStream OutputStream IOException]
           [java.nio.charset StandardCharsets]))

(defn- read-ascii-line [^InputStream in]
  (let [sb (StringBuilder.)]
    (loop [carriage-return false]
      (let [ch (.read in)]
        (if (= -1 ch)
          (if (zero? (.length sb)) nil (.toString sb))
          (let [ch (char ch)]
            (.append sb ch)
            (cond
              (= ch \return) (recur true)
              (and carriage-return (= ch \newline)) (.substring sb 0 (- (.length sb) 2))
              :else (recur false))))))))

(defn- consume-in! [^InputStream in ch err-ch]
  (lsp.async/supply-blocking
    (fn []
      (try
        (when-some [headers (loop [acc {}]
                              (let [line (read-ascii-line in)]
                                (cond
                                  (nil? line) nil
                                  (= "" line) acc
                                  :else (if-let [[_ field value] (re-matches #"^([^:]+):\s*(.+?)\s*$" line)]
                                          (recur (assoc acc (string/lower-case field) value))
                                          (throw (ex-info (str "Can't parse the header: " line) {:line line}))))))]
          ;; we have headers
          (let [len (Integer/valueOf ^String (or (get headers "content-length")
                                                 (throw (ex-info "Required header missing: Content-Length" headers))))
                bytes (.readNBytes in len)]
            (if (= (alength bytes) len)
              (String. bytes StandardCharsets/UTF_8)
              (throw (ex-info "Couldn't read enough bytes" {:expected len :actual (alength bytes)})))))
        (catch IOException _)
        (catch Throwable e
          (a/put! err-ch e)
          (a/close! err-ch)
          nil)))
    ch))

(defn- pipe-out! [^OutputStream out ch]
  (let [content-length-message-bytes (.getBytes "Content-Length: " StandardCharsets/US_ASCII)
        content-type-message-bytes (.getBytes "\r\nContent-Type: application/vscode-jsonrpc; charset=utf-8\r\n\r\n" StandardCharsets/US_ASCII)]
    (lsp.async/drain-blocking
      (fn [^String message]
        (try
          (let [message-bytes (.getBytes message StandardCharsets/UTF_8)
                content-length-value-bytes (.getBytes (str (alength message-bytes)) StandardCharsets/US_ASCII)]
            (doto out
              (.write content-length-message-bytes)
              (.write content-length-value-bytes)
              (.write content-type-message-bytes)
              (.write message-bytes)
              .flush))
          (catch IOException _)
          (catch Throwable e (error-reporting/report-exception! e))))
      ch)))

(defn make
  "Start base LSP communication protocol on in and out streams

  Args:
    in     an InputStream to read incoming string messages from
    out    an OutputStream to write outgoing string messages to

  Returns a triplet of channels - source, sink and err - representing the base
  layer connection:
    - takes from the source channel will return strings from the language
      server, then, when the input stream closes, the source channel will close
    - string puts to the sink channel will be sent to the language server
    - if error occurs during read due to a faulty server, err channel will
      receive the exception and then will be closed alongside with the source
      channel

  See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#baseProtocol"
  [^InputStream in ^OutputStream out]
  (let [sink (a/chan 128)
        source (a/chan 128)
        err (a/chan 1)]
    (consume-in! in source err)
    (pipe-out! out sink)
    [source sink err]))