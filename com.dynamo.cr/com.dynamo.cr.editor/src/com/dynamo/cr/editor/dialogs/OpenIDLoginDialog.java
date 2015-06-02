package com.dynamo.cr.editor.dialogs;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintWriter;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URLEncoder;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.ws.rs.core.UriBuilder;

import org.apache.commons.io.IOUtils;
import org.eclipse.jetty.server.Request;
import org.eclipse.jetty.server.Server;
import org.eclipse.jetty.server.bio.SocketConnector;
import org.eclipse.jetty.server.handler.AbstractHandler;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.dialogs.IDialogConstants;
import org.eclipse.swt.SWT;
import org.eclipse.swt.browser.Browser;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Shell;

import com.dynamo.cr.common.providers.JsonProviders;
import com.dynamo.cr.common.providers.ProtobufProviders;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.protocol.proto.Protocol.TokenExchangeInfo;
import com.dynamo.cr.protocol.proto.Protocol.TokenExchangeInfo.Type;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.ClientHandlerException;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;

public class OpenIDLoginDialog extends Dialog {

    private Server server;
    private SocketConnector connector;
    private String crServerUri;
    private String email;
    private String authToken;

    private void writeResponse(PrintWriter writer, String response) {
        writer.println("<body>");
        writer.println(response);
        writer.println("<body>");
    }

    public OpenIDLoginDialog(Shell parentShell, final String crServerUri) {
        super(parentShell);
        this.crServerUri = crServerUri;
        server = new Server();
        connector = new SocketConnector();
        connector.setPort(0);
        server.addConnector(connector);

        server.setHandler(new AbstractHandler() {
            @Override
            public void handle(String target, Request baseRequest, HttpServletRequest request,
                    HttpServletResponse response) throws IOException, ServletException {
                response.setContentType("text/html");
                response.setStatus(HttpServletResponse.SC_OK);

                PrintWriter writer = response.getWriter();
                writer.println("<head>");
                writer.println("<style type=\"text/css\">");
                writer.println("body {");
                writer.println("font-family: Arial Unicode MS, Arial, sans-serif;");
                writer.println("font-size: small;");
                writer.println("</style>");
                writer.println("</head>");

                ((Request)request).setHandled(true);

                ClientConfig cc = new DefaultClientConfig();
                cc.getClasses().add(JsonProviders.ProtobufMessageBodyReader.class);
                cc.getClasses().add(JsonProviders.ProtobufMessageBodyWriter.class);
                cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
                cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

                Pattern re = Pattern.compile("/(.*?)/(.*?)");
                Matcher matcher = re.matcher(request.getRequestURI());
                if (!matcher.matches()) {
                    writeResponse(writer, "Internal error");
                    ((Request)request).setHandled(true);
                    return;
                }

                String loginToken = matcher.group(1);
                String action = matcher.group(2);

                if (action.equals("cancel")) {
                    getShell().getDisplay().asyncExec(new Runnable() {
                        @Override
                        public void run() {
                            cancelPressed();
                        }
                    });
                    return;
                }

                Client client = Client.create(cc);
                URI uri = UriBuilder.fromUri(crServerUri).build();
                WebResource webResource = client.resource(uri);
                TokenExchangeInfo exchangeInfo;
                try {
                    exchangeInfo = webResource.path("/login/oauth/exchange").path(loginToken).accept(ProtobufProviders.APPLICATION_XPROTOBUF_TYPE).get(TokenExchangeInfo.class);
                } catch (ClientHandlerException e) {
                    writeResponse(writer, e.getMessage());
                    return;
                }

                if (exchangeInfo.getType() == Type.SIGNUP) {
                    writeResponse(writer, "This account is not associated with defold.com yet. Please go to defold.com to signup");
                    return;
                }

                OpenIDLoginDialog.this.email = exchangeInfo.getEmail();
                OpenIDLoginDialog.this.authToken = exchangeInfo.getAuthToken();

                getShell().getDisplay().asyncExec(new Runnable() {
                    @Override
                    public void run() {
                        okPressed();
                    }
                });
            }
        });

        try {
            server.start();
        } catch (Exception e) {
            Activator.logException(e);
        }
    }

    public String getEmail() {
        return email;
    }

    public String getAuthToken() {
        return authToken;
    }

    @Override
    protected void createButtonsForButtonBar(Composite parent) {
        createButton(parent, IDialogConstants.CANCEL_ID,
                IDialogConstants.CANCEL_LABEL, false);
    }

    @Override
    public boolean close() {
        try {
            server.stop();
            connector.stop();
        } catch (Exception e) {
            Activator.logException(e);
        }
        return super.close();
    }

    @Override
    protected void configureShell(Shell newShell) {
        super.configureShell(newShell);
        newShell.setText("Sign in to defold.com");
    }

    @Override
    protected Control createContents(Composite parent) {
        Control ret = super.createContents(parent);
        return ret;
    }

    @Override
    protected boolean isResizable() {
        return true;
    }

    String encode(String string) {
        try {
            return URLEncoder.encode(string, "UTF-8");
        } catch (UnsupportedEncodingException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    protected Control createDialogArea(Composite parent) {
        Browser browser = new Browser(parent, SWT.NONE);
        UriBuilder googleUrl = UriBuilder.fromUri(crServerUri).path("/login/oauth/google").queryParam("redirect_to", String.format("http://localhost:%d/{token}/{action}", connector.getLocalPort()));
        UriBuilder yahooUrl = UriBuilder.fromUri(crServerUri).path("/login/oauth/yahoo").queryParam("redirect_to", String.format("http://localhost:%d/{token}/{action}", connector.getLocalPort()));

        InputStream input = getClass().getClassLoader().getResourceAsStream("/data/login.html");
        ByteArrayOutputStream output = new ByteArrayOutputStream(128 * 1024);
        try {
            IOUtils.copy(input, output);
            String text = new String(output.toByteArray(), "UTF8");
            text = text.replace("GOOGLE_URL", googleUrl.build().toString());
            text = text.replace("YAHOO_URL", yahooUrl.build().toString());
            browser.setText(text);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        browser.setLayoutData(new GridData(GridData.FILL_BOTH));
        return parent;
    }

    @Override
    protected Point getInitialSize() {
        return new Point(900, 800);
    }

    public static void main(String[] args) {
        Display display = new Display();
        Shell shell = new Shell(display);
        shell.setLayout(new FillLayout());
        shell.setText("Test OpenID login");
        shell.open();

        OpenIDLoginDialog dialog = new OpenIDLoginDialog(shell, "http://localhost:9998");
        int ret = dialog.open();

        if (ret == Dialog.OK) {
            System.out.println(dialog.getEmail());
            System.out.println(dialog.getAuthToken());
        } else {

        }

        while (!shell.isDisposed()) {
            if (!display.readAndDispatch())
                display.sleep();
        }
        display.dispose();
    }

}
