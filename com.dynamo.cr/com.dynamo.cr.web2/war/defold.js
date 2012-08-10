function getServerUrl() {
    var url = getCookies().url;
    if (!url) {
        url = 'http://cr.defold.com:9998';
    }
    return url;
}

function trackPage(pageName) {
    try {
        if (_gaq) {
            _gaq.push([ '_trackPageview', pageName ]);
        }
    } catch (err) {
    }
}

function signUp() {
	var email = $("#email").val();
	$.ajax({
		type : "PUT",
		url : getServerUrl() + "/prospects/" + email
	})
	$("#email").val("");
	$("#signUp").html('<h3>Signed up!</h3>');
	trackPage('signup');
	return false;
}

function compatRedirect() {
	/* Client side redirection for compatibility with old URL:s */
	var href = window.location.href;
	if (href.match(/.*?#[!]?blog[:]?$/)) {
		window.location.href = "/blog";
	} else if (href.match(/.*?#[!]?documentation[:]?$/)) {
		window.location.href = "/doc";
	} else if (href.match(/.*?#[!]?guide:(.*?)$/)) {
		window.location.href = "/doc/" + RegExp.$1;
	} else if (href.match(/.*?#[!]?tutorials:(.*?)$/)) {
		window.location.href = "/doc/" + RegExp.$1;
	} else if (href.match(/.*?#[!]?script_sample:(.*?)$/)) {
		window.location.href = "/doc/" + RegExp.$1;
	} else if (href.match(/.*?#[!]?content:(.*?)$/)) {
		window.location.href = "/about/" + RegExp.$1;
	} else if (href.match(/.*?#[!]?reference:(.*?)\/(.*?)$/)) {
		window.location.href = "/ref/" + RegExp.$1 + "#" + RegExp.$2;
	}
}

function getCookies() {
    var d = {};
    var lst = document.cookie.split("; ");
    for ( var i in lst) {
        var keyValue = lst[i].split("=");

        d[keyValue[0]] = decodeURIComponent(keyValue[1]);
    }
    return d;
}

function setCookie(key, value) {
    document.cookie = key + "=" + encodeURIComponent(value) + "; path=/";
}

function clearCookie(key) {
    document.cookie = key + "=; expires=Thu, 01 Jan 1970 00:00:00 GMT; path=/";
}
