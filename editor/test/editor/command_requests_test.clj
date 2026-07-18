;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.

(ns editor.command-requests-test
  (:require [clojure.test :refer :all]
            [editor.command-requests :as command-requests]
            [util.http-server :as http-server]))

(deftest run-request-user-data-test
  (let [request-user-data #'command-requests/run-request-user-data]
    (are [expected request] (= expected (request-user-data request))
      {} {}
      {} {:query ""}
      {:focus true} {:query "focus=true"}
      {:focus false} {:query "focus=false"}
      {:focus false} {:query "other=value&focus=false"})
    (doseq [query ["focus=" "focus=invalid" "focus=TRUE"]]
      (let [error (try
                    (request-user-data {:query query})
                    nil
                    (catch Exception error
                      error))]
        (is (some? error))
        (is (= 400 (-> error ex-data ::http-server/response :status)))))))

(deftest focus-query-is-run-only-test
  (let [supported-commands @#'command-requests/supported-commands]
    (is (fn? (get-in supported-commands [:run :request->user-data])))
    (is (nil? (get-in supported-commands [:compile :request->user-data])))))
