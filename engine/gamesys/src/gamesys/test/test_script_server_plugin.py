
def do_GET(req):

    result = {}
    result['to_send'] = ""
    result['headers'] = {}

    if req.startswith('/test_progress'):
        result['to_send'] = "test_progress_complete"
        if req == '/test_progress_content_length':
            result['headers']['Content-Length'] = len(result['to_send'])
        elif req == '/test_progress_chunked':
            chunk_size = 64 * 1024
            chunk_count = 2
            result['to_send'] = 'A' * chunk_count * chunk_size
            result['headers']['Content-Length'] = chunk_count * chunk_size

    return result
