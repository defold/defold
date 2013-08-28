In app purchaes
===============


General
-------

**TODO**

* Callback to iap.restore() (success / failure). Currently we only invoke the iap-listener callback and we don't know when we are done (zero items, etc)
* Review code
* Fix global variable hack
* Fix global callback hack (context-data for list(), etc)

Android
-------

The file IInAppBillingService.java is generated from IInAppBillingService.aidl using eclipse.

**TODO**

* All products are automatically consumed. This might not work with subsriptions.
* Include android specific fields in product and purchase?
* Signature validation support (developerPayload)

