
var LibraryFacebookIAP = {
        $FBinner: {
            // NOTE: Also defined in iap.h
            TransactionState :
            {
                    TRANS_STATE_PURCHASING : 0,
                    TRANS_STATE_PURCHASED : 1,
                    TRANS_STATE_FAILED : 2,
                    TRANS_STATE_RESTORED : 3,
                    TRANS_STATE_UNVERIFIED : 4
            },

            // NOTE: Also defined in iap.h
            BillingResponse :
            {
                BILLING_RESPONSE_RESULT_OK : 0,
                BILLING_RESPONSE_RESULT_USER_CANCELED : 1,
                BILLING_RESPONSE_RESULT_BILLING_UNAVAILABLE : 3,
                BILLING_RESPONSE_RESULT_ITEM_UNAVAILABLE : 4,
                BILLING_RESPONSE_RESULT_DEVELOPER_ERROR : 5,
                BILLING_RESPONSE_RESULT_ERROR : 6,
                BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED : 7,
                BILLING_RESPONSE_RESULT_ITEM_NOT_OWNED : 8
            },

            // See Facebook definitions https://developers.facebook.com/docs/payments/reference/errorcodes
            FBPaymentResponse :
            {
                FB_PAYMENT_RESPONSE_USERCANCELED : 1383010,
                FB_PAYMENT_RESPONSE_APPINVALIDITEMPARAM : 1383051
            },

            http_callback: function(xmlhttp, callback, lua_state, products, product_ids, product_count, url_index, url_count) {
                if (xmlhttp.readyState == 4) {
                    if(xmlhttp.status == 200) {
                        var xmlDoc = document.createElement( 'html' );
                        xmlDoc.innerHTML = xmlhttp.responseText;
                        var	elements = xmlDoc.getElementsByTagName('meta');

                        var productInfo = {};
                        for (var i=0; i<elements.length; i++) {
                            if(elements[i].getAttribute("property") == "og:url") {
                                productInfo.ident = elements[i].getAttribute("content");
                                continue;
                            }
                            if(elements[i].getAttribute("property") == "og:title") {
                                productInfo.title = elements[i].getAttribute("content");
                                continue;
                            }
                            if(elements[i].getAttribute("property") == "og:description") {
                                productInfo.description = elements[i].getAttribute("content");
                                continue;
                            }
                            if(elements[i].getAttribute("property") == "product:price:amount") {
                                productInfo.price = elements[i].getAttribute("content");
                                continue;
                            }
                            if(elements[i].getAttribute("property") == "product:price:currency") {
                                productInfo.currency_code = elements[i].getAttribute("content");
                                continue;
                            }
                        }
                        productInfo.price_string = productInfo.price + productInfo.currency_code;

                        products[product_ids[url_index]] = productInfo;

                    } else {
                        products[product_ids[url_index]] = "";
                    }

                    if(url_index == product_count-1) {
                        var productsJSON = JSON.stringify(products);
                        var res_buf = allocate(intArrayFromString(productsJSON), 'i8', ALLOC_STACK);
                        Runtime.dynCall('vii', callback, [lua_state, res_buf]);
                    } else {
                        var xmlhttp = new XMLHttpRequest();
                        xmlhttp.onreadystatechange = function() {
                            FBinner.http_callback(xmlhttp, callback, lua_state, products, product_ids, product_count, url_index+1);
                        };
                        xmlhttp.open("GET", product_ids[url_index+1], true);
                        xmlhttp.send();
                    }
                }
            },

        },

        dmIAPFBList: function(params, callback, lua_state) {
            var product_ids = Pointer_stringify(params).trim().split(',');
            var product_count = product_ids.length;
            if(product_count == 0) {
                console.log("Calling iap.list with no item id's. Ignored.");
                return;
            }
            products = {};
            var xmlhttp = new XMLHttpRequest();
            xmlhttp.onreadystatechange = function() {
                FBinner.http_callback(xmlhttp, callback, lua_state, products, product_ids, product_count, 0);
            };
            // chain of async http read of product files
            xmlhttp.open("GET", product_ids[0], true);
            xmlhttp.send();
        },

        // https://developers.facebook.com/docs/javascript/reference/FB.ui
        dmIAPFBBuy: function(param_product_id, param_request_id, callback, lua_state) {
            var product_id = Pointer_stringify(param_product_id);

            var buy_params = {
                    method: 'pay',
                    action: 'purchaseitem',
                    product: product_id,
            };
            if(param_request_id != 0) {
                buy_params.request_id = Pointer_stringify(param_request_id);
            }

            FB.ui(buy_params,
            	function(response) {
	                if(response && response.status) {
	                    var result = {};
	                    result.ident = product_id;
	                    var currentDate = new Date();
	                    result.date = currentDate.toISOString();

	                    if (response.status == 'initiated') {
	                        result.state = FBinner.TransactionState.TRANS_STATE_UNVERIFIED;
	                        result.trans_ident = response.payment_id.toString();
	                        result.receipt = response.signed_request;
	                        result.request_id = response.request_id;
	                    } else if (response.status == 'completed') {
	                        result.state = FBinner.TransactionState.TRANS_STATE_PURCHASED;
	                        result.trans_ident = response.payment_id.toString();
	                        result.receipt = response.signed_request;
	                        result.request_id = response.request_id;
	                    } else {
	                        // unknown and 'failed' state
	                        if (response.status != 'failed') {
	                            console.log("Unknown response status (default to 'failed'): ", response.status);
	                        }
	                        result.state = FBinner.TransactionState.TRANS_STATE_FAILED;
	                    }

	                    var productsJSON = JSON.stringify(result)
	                    var res_buf = allocate(intArrayFromString(productsJSON), 'i8', ALLOC_STACK);
	                    Runtime.dynCall('viii', callback, [lua_state, res_buf, 0]);

	                } else {

	                    var reason;
	                    if(!response || response.error_code == FBinner.FBPaymentResponse.FB_PAYMENT_RESPONSE_USERCANCELED) {
	                        reason = FBinner.BillingResponse.BILLING_RESPONSE_RESULT_USER_CANCELED;
	                    } else if (response.error_code == FBinner.FBPaymentResponse.FB_PAYMENT_RESPONSE_APPINVALIDITEMPARAM) {
	                        reason = FBinner.BillingResponse.BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED;
	                    } else {
	                        reason = FBinner.BillingResponse.BILLING_RESPONSE_RESULT_ERROR;
	                        console.log("Unknown response: ", response);
	                    }
	                    Runtime.dynCall('viii', callback, [lua_state, 0, reason]);
	                }
            	}
            );
        },

}

autoAddDeps(LibraryFacebookIAP, '$FBinner');
mergeInto(LibraryManager.library, LibraryFacebookIAP);
