package com.dynamo.cr.server;

/**
 * Copyright (C) 2009 Mo Chen <withinsea@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import java.io.IOException;
import java.io.UnsupportedEncodingException;

import javax.servlet.Filter;
import javax.servlet.FilterChain;
import javax.servlet.FilterConfig;
import javax.servlet.ServletException;
import javax.servlet.ServletRequest;
import javax.servlet.ServletResponse;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

public class JsonpFilter implements Filter {

	protected String jsonp = "callback";
	protected String[] jsonMimeTypes = new String[] {
		"application/json",
		"application/x-json",
		"text/json",
		"text/x-json"
	};

	protected boolean isJsonp(ServletRequest req) {
		if (req instanceof HttpServletRequest) {
			return req.getParameterMap().containsKey(jsonp);
		} else {
			return false;
		}
	}

	protected boolean isJson(ServletRequest req, ServletResponse resp) {
		String ctype = resp.getContentType();
		if (ctype == null || ctype.equals("")) {
			return false;
		}
		for (String jsonMimeType : jsonMimeTypes) {
			if (ctype.indexOf(jsonMimeType) >= 0) {
				return true;
			}
		}
		return false;
	}

	protected String getCallback(ServletRequest req) {
		return req.getParameterValues(jsonp)[0];
	}

	public void doFilter(final ServletRequest req, final ServletResponse resp,
			FilterChain chain) throws IOException, ServletException {

		if (resp instanceof HttpServletResponse && isJsonp(req)) {

			HttpServletResponseContentWrapper wrapper = new HttpServletResponseContentWrapper(
					(HttpServletResponse) resp) {

				private boolean wrapContentType = false;

				public String getContentType() {
					return wrapContentType ? "text/javascript; charset=utf-8" :
						super.getContentType();
				}

				@Override
				public byte[] wrap(byte[] content) throws UnsupportedEncodingException {
					wrapContentType = true;
					String contentstr = new String(content, getCharacterEncoding());
					boolean isJson = isJson(req, super.getResponse());
					return (getCallback(req) + "(" +
						(isJson ? contentstr : quote(contentstr)) +
					");").getBytes(getCharacterEncoding());
				}
			};

			wrapper.setCharacterEncoding("UTF-8");
			chain.doFilter(req, wrapper);
			wrapper.flushWrapper();

		} else {

			chain.doFilter(req, resp);

		}
	}

	public void init(FilterConfig config) throws ServletException {
		String jsonp = config.getInitParameter("jsonp");
		String jsonMimeTypes = config.getInitParameter("json-mime-types");
		if (jsonp != null && !jsonp.equals("")) {
			this.jsonp = jsonp;
		}
		if (jsonMimeTypes != null) {
			if (jsonMimeTypes.equals("")) {
				this.jsonMimeTypes = new String [] { };
			} else {
				this.jsonMimeTypes = jsonMimeTypes.trim().split("\\s*,\\s*");
			}
		}
	}

	public void destroy() {

	}

	/**
	 * copy from: org.json, org.json.JSONObject.quote(String string)
	 */
	private static String quote(String string) {
		if (string == null || string.length() == 0) {
			return "\"\"";
		}

		char b;
		char c = 0;
		int i;
		int len = string.length();
		StringBuffer sb = new StringBuffer(len + 4);
		String t;

		sb.append('"');
		for (i = 0; i < len; i += 1) {
			b = c;
			c = string.charAt(i);
			switch (c) {
			case '\\':
			case '"':
				sb.append('\\');
				sb.append(c);
				break;
			case '/':
				if (b == '<') {
					sb.append('\\');
				}
				sb.append(c);
				break;
			case '\b':
				sb.append("\\b");
				break;
			case '\t':
				sb.append("\\t");
				break;
			case '\n':
				sb.append("\\n");
				break;
			case '\f':
				sb.append("\\f");
				break;
			case '\r':
				sb.append("\\r");
				break;
			default:
				if (c < ' ' || (c >= '\u0080' && c < '\u00a0')
						|| (c >= '\u2000' && c < '\u2100')) {
					t = "000" + Integer.toHexString(c);
					sb.append("\\u" + t.substring(t.length() - 4));
				} else {
					sb.append(c);
				}
			}
		}
		sb.append('"');
		return sb.toString();
	}
}