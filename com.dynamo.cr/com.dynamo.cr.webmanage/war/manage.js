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

function showError(message) {
    $("#main").hide();
    $("#errorMessage").html(message);
    $("#errorAlert").show();
}

function getServerUrl() {
    var url = getCookies().url;
    if (!url) {
        url = 'http://cr.defold.com:9998';
    }
    return url;
}

function setServerUrl(url) {
    setCookie('url', url);
}

function sendRequest(method, path, callback) {
    var url = getServerUrl() + path;
    var req = new XMLHttpRequest();
    req.onreadystatechange = function() {
        if (req.readyState == 4) {
            if (req.status == 200) {
                callback(req);
            } else {
                showError(req.response + " (" + req.status + ")");
            }
        }
    }
    var cookies = getCookies();
    req.open(method, url);
    req.setRequestHeader('X-Auth', cookies.auth);
    req.setRequestHeader('X-Email', cookies.email);
    req.setRequestHeader('Accept', 'application/json');
    req.send();
}

function login() {
    var tokenRe = /.*?\?token=(.*?)$/;
    if (tokenRe.exec(document.location.href)) {
        var token = RegExp.$1;
        $.getJSON(getServerUrl() + '/login/openid/exchange/' + token, function(info) {
            setCookie('auth', info.auth_cookie);
            setCookie('email', info.email);
            setCookie('first_name', info.first_name);
            setCookie('last_name', info.last_name);
            window.location.replace(window.location.origin);
        });
    } else {
        var req = new XMLHttpRequest();
        req.onreadystatechange = function() {
            if (req.readyState == 4) {
            }
        }

        var redirectTo = window.location.origin + window.location.pathname;
        var uri = getServerUrl() + "/login/openid/google?redirect_to=" + redirectTo + "?token={token}";
        document.location.replace(uri);
    }
}

function loadRecipients() {
    sendRequest("GET", "/news_list", function(x) {
        var subscribers = JSON.parse(x.responseText);
        var text = "";
        for (i in subscribers.subscribers) {
            var s = subscribers.subscribers[i];
            text = text + s.email + ', ' + s.key + '\n';
        }
        $("#recipients").val(text);
    });
}

function sendNewsMail() {
    var subject = $('#subject').val();
    var recipients = $('#recipients').val();
    var from = $('#from').val();
    var name = $('#name').val();
    var message = $('#message').val();
    $.post('/send-news-mail', { subject: subject,
                                recipients: recipients,
                                from: from,
                                name: name,
                                message: message,
                                serverUrl: getServerUrl() });
    $('#message-sent-label').show();
    return false;
}
