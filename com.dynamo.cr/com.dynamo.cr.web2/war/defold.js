function signUp() {
	var email = $("#email")[0].value;
	$.ajax({
		type : "PUT",
		url : "http://cr.defold.com:9998/prospects/" + email
	})
	$("#email").val("");
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
