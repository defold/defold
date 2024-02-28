
def do_GET(req):
	if req.startswith('/test_progress'):
		return "test_progress_complete"

	return ""
