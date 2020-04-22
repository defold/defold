import mimetypes

def get(url, token):
    import requests
    try:
        response = requests.get(url, headers={"Authorization": "token %s" % (token)})
        response.raise_for_status()
        return response.json()
    except Exception as err:
        print(err)
        return None

def post(url, token, data = None, json = None, files = None, headers = None):
    import requests
    try:
        if not headers:
            headers = {}
        headers["Authorization"] = "token %s" % (token)

        response = requests.post(url, data = data, json = json, files = files, headers=headers)
        response.raise_for_status()
        return response.json()
    except Exception as err:
        print(err)
        return None

def delete(url, token):
    import requests
    try:
        response = requests.delete(url, headers={"Authorization": "token %s" % (token)})
        response.raise_for_status()
        if response.content and response.content != "":
            return response.json()
        else:
            return None
    except Exception as err:
        print(err)
        return None
