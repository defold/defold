In app purchaes
===============


General
-------

**TODO**

* Callback to iap.restore() (success / failure). Currently we only invoke the iap-listener callback and we don't know when we are done (zero items, etc)
* Review code
* Fix global variable hack
* Fix global callback hack (context-data for list(), etc)
* Error codes to error table

Android
-------

The file IInAppBillingService.java is generated from IInAppBillingService.aidl using eclipse.

**TODO**

* service might be null. See Log.wtf(TAG, "service is null") in Iap.java. Wait for service?
* All products are automatically consumed. This might not work with subsriptions.
* Include android specific fields in product and purchase?
* Signature validation support (developerPayload)

