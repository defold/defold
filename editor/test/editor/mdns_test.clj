;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.mdns-test
  (:require [clojure.test :refer :all])
  (:import [com.dynamo.discovery MDNS MDNS$Logger MDNSServiceInfo]
           [java.io ByteArrayOutputStream]
           [java.lang.reflect Method]
           [java.net DatagramPacket InetAddress MulticastSocket]))

(set! *warn-on-reflection* true)

(def ^:private dns-type-ptr 12)
(def ^:private dns-type-txt 16)
(def ^:private dns-type-a 1)
(def ^:private dns-type-srv 33)
(def ^:private dns-class-in 1)

(defn- utf8-bytes
  ^bytes [^String s]
  (.getBytes s "UTF-8"))

(defn- write-u16! [^ByteArrayOutputStream out ^long n]
  (.write out (bit-and (bit-shift-right n 8) 0xff))
  (.write out (bit-and n 0xff)))

(defn- write-u32! [^ByteArrayOutputStream out ^long n]
  (.write out (bit-and (bit-shift-right n 24) 0xff))
  (.write out (bit-and (bit-shift-right n 16) 0xff))
  (.write out (bit-and (bit-shift-right n 8) 0xff))
  (.write out (bit-and n 0xff)))

(defn- write-name! [^ByteArrayOutputStream out ^String name]
  (doseq [^String label (.split name "\\.")]
    (when-not (.isEmpty label)
      (let [^bytes b (utf8-bytes label)]
        (.write out (alength b))
        (.write out b 0 (alength b)))))
  (.write out 0))

(defn- make-record
  ^bytes [^String name ^long record-type ^long ttl ^bytes rdata]
  (let [out (ByteArrayOutputStream.)]
    (write-name! out name)
    (write-u16! out record-type)
    (write-u16! out dns-class-in)
    (write-u32! out ttl)
    (write-u16! out (alength rdata))
    (.write out rdata 0 (alength rdata))
    (.toByteArray out)))

(defn- make-ptr-rdata ^bytes [^String full-service-name]
  (let [out (ByteArrayOutputStream.)]
    (write-name! out full-service-name)
    (.toByteArray out)))

(defn- make-srv-rdata ^bytes [^long port ^String host-name]
  (let [out (ByteArrayOutputStream.)]
    (write-u16! out 0) ; priority
    (write-u16! out 0) ; weight
    (write-u16! out port)
    (write-name! out host-name)
    (.toByteArray out)))

(defn- make-txt-rdata ^bytes [entries]
  (let [out (ByteArrayOutputStream.)]
    (doseq [^String entry entries]
      (let [^bytes b (utf8-bytes entry)]
        (.write out (alength b))
        (.write out b 0 (alength b))))
    (.toByteArray out)))

(defn- make-a-rdata ^bytes [a b c d]
  (byte-array [(byte a) (byte b) (byte c) (byte d)]))

(defn- make-response-packet
  ^bytes [records]
  (let [out (ByteArrayOutputStream.)]
    (write-u16! out 0) ; id
    (write-u16! out 0x8400) ; response
    (write-u16! out 0) ; questions
    (write-u16! out (count records)) ; answers
    (write-u16! out 0) ; ns
    (write-u16! out 0) ; ar
    (doseq [^bytes record records]
      (.write out record 0 (alength record)))
    (.toByteArray out)))

(defn- invoke-private!
  [obj ^String method-name param-types & args]
  (let [^Method m (.getDeclaredMethod (class obj) method-name (into-array Class param-types))]
    (.setAccessible m true)
    (.invoke m obj (object-array args))))

(defn- parse-and-rebuild!
  [^MDNS mdns ^bytes packet]
  (invoke-private! mdns "parsePacket" [(Class/forName "[B") Integer/TYPE String] packet (alength packet) "127.0.0.1")
  (invoke-private! mdns "rebuildDiscovered" []))

(defn- service-records
  [service-type full-name host-name port ttl]
  [(make-record service-type dns-type-ptr ttl (make-ptr-rdata full-name))
   (make-record full-name dns-type-srv ttl (make-srv-rdata port host-name))
   (make-record full-name dns-type-txt ttl (make-txt-rdata ["id=test-id" "name=Test Device" "log_port=7001" "schema=1"]))
   (make-record host-name dns-type-a ttl (make-a-rdata 127 0 0 1))])

(defn- dummy-logger
  ^MDNS$Logger []
  (reify MDNS$Logger
    (log [_ _] nil)))

(deftest mdns-discovers-service-from-dns-sd-records
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetOne." service-type)
        host-name "target-host.local"
        packet (make-response-packet (service-records service-type full-name host-name 8123 120))]
    (parse-and-rebuild! mdns packet)
    (let [devices ^"[Lcom.dynamo.discovery.MDNSServiceInfo;" (.getDevices mdns)]
      (is (= 1 (alength devices)))
      (let [^MDNSServiceInfo d (aget devices 0)]
        (is (= "test-id" (.id d)))
        (is (= "Test Device" (.instanceName d)))
        (is (= full-name (.serviceName d)))
        (is (= host-name (.host d)))
        (is (= "127.0.0.1" (.address d)))
        (is (= 8123 (.port d)))
        (is (= "7001" (.logPort d)))
        (is (= "1" (get (.txt d) "schema")))))))

(deftest mdns-removes-service-on-zero-ttl-records
  (let [mdns (MDNS. (dummy-logger))
        service-type MDNS/MDNS_SERVICE_TYPE
        full-name (str "TargetTwo." service-type)
        host-name "target-host-2.local"
        add-packet (make-response-packet (service-records service-type full-name host-name 9001 120))
        remove-packet (make-response-packet (service-records service-type full-name host-name 9001 0))]
    (parse-and-rebuild! mdns add-packet)
    (is (= 1 (alength (.getDevices mdns))))
    (parse-and-rebuild! mdns remove-packet)
    (is (= 0 (alength (.getDevices mdns))))))

(deftest mdns-live-multicast-integration
  (let [mdns (MDNS. (dummy-logger))]
    (if-not (.setup mdns)
      (is true "Skipping live multicast test: setup failed")
      (try
        (if-not (.isConnected mdns)
          (is true "Skipping live multicast test: no multicast interfaces")
          (let [service-type MDNS/MDNS_SERVICE_TYPE
                full-name (str "TargetLive." service-type)
                host-name "target-live.local"
                add-packet (make-response-packet (service-records service-type full-name host-name 9011 120))
                remove-packet (make-response-packet (service-records service-type full-name host-name 9011 0))
                group (InetAddress/getByName "224.0.0.251")
                socket (MulticastSocket.)]
           (try
              (try
                (.send socket (DatagramPacket. add-packet (alength add-packet) group 5353))
                (loop [i 0]
                  (when (and (< i 200) (zero? (alength (.getDevices mdns))))
                    (.update mdns false)
                    (Thread/sleep 10)
                    (recur (inc i))))
                (is (= 1 (alength (.getDevices mdns))))
                (let [^MDNSServiceInfo d (aget (.getDevices mdns) 0)]
                  (is (= full-name (.serviceName d)))
                  (is (= 9011 (.port d))))

                (.send socket (DatagramPacket. remove-packet (alength remove-packet) group 5353))
                (loop [i 0]
                  (when (and (< i 200) (pos? (alength (.getDevices mdns))))
                    (.update mdns false)
                    (Thread/sleep 10)
                    (recur (inc i))))
                (is (= 0 (alength (.getDevices mdns))))
                (catch java.io.IOException _
                  (is true "Skipping live multicast test: multicast send unavailable in this environment")))
              (finally
                (.close socket)))))
        (finally
          (.dispose mdns))))))
