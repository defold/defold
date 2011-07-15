package com.dynamo.cr.editor.dialogs;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URLEncoder;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.ws.rs.core.UriBuilder;

import org.eclipse.jetty.server.Request;
import org.eclipse.jetty.server.Server;
import org.eclipse.jetty.server.bio.SocketConnector;
import org.eclipse.jetty.server.handler.AbstractHandler;
import org.eclipse.jface.dialogs.Dialog;
import org.eclipse.jface.dialogs.IDialogConstants;
import org.eclipse.jface.dialogs.TitleAreaDialog;
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
import com.dynamo.cr.protocol.proto.Protocol.TokenExchangeInfo;
import com.dynamo.cr.protocol.proto.Protocol.TokenExchangeInfo.Type;
import com.sun.jersey.api.client.Client;
import com.sun.jersey.api.client.WebResource;
import com.sun.jersey.api.client.config.ClientConfig;
import com.sun.jersey.api.client.config.DefaultClientConfig;

public class OpenIDLoginDialog extends TitleAreaDialog {

    private Server server;
    private SocketConnector connector;
    private String crServerUri;
    private String email;
    private String authCookie;

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
                ((Request)request).setHandled(true);

                ClientConfig cc = new DefaultClientConfig();
                cc.getClasses().add(JsonProviders.ProtobufMessageBodyReader.class);
                cc.getClasses().add(JsonProviders.ProtobufMessageBodyWriter.class);
                cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyReader.class);
                cc.getClasses().add(ProtobufProviders.ProtobufMessageBodyWriter.class);

                Pattern re = Pattern.compile("/(.*?)/(.*?)");
                Matcher matcher = re.matcher(request.getRequestURI());
                if (!matcher.matches()) {
                    response.getWriter().println("Internal error");
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
                URI uri = UriBuilder.fromUri(crServerUri).port(9998).build();
                WebResource webResource = client.resource(uri);
                TokenExchangeInfo exchangeInfo = webResource.path("/login/openid/exchange").path(loginToken).accept(ProtobufProviders.APPLICATION_XPROTOBUF_TYPE).get(TokenExchangeInfo.class);

                if (exchangeInfo.getType() == Type.SIGNUP) {
                    response.getWriter().println("This account is not associated with defold.se yet. Please go to defold.se to signup");
                    return;
                }

                OpenIDLoginDialog.this.email = exchangeInfo.getEmail();
                OpenIDLoginDialog.this.authCookie = exchangeInfo.getAuthCookie();

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
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
    }

    public String getEmail() {
        return email;
    }

    public String getAuthCookie() {
        return authCookie;
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
            e.printStackTrace();
        }
        return super.close();
    }

    @Override
    protected void configureShell(Shell newShell) {
        super.configureShell(newShell);
        newShell.setText("Sign in to defold.se");
    }

    @Override
    protected Control createContents(Composite parent) {
        Control ret = super.createContents(parent);
        setTitle("Sign in");
        setMessage("Sign in defold.se using your Google account");
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
        UriBuilder uri = UriBuilder.fromUri(crServerUri).path("/login/openid/google").queryParam("redirect_to", String.format("http://localhost:%d/{token}/{action}", connector.getLocalPort()));

        browser.setUrl(uri.build().toString());
        browser.setLayoutData(new GridData(GridData.FILL_BOTH));
        return parent;
    }

    @Override
    protected Point getInitialSize() {
        return new Point(700, 800);
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
            System.out.println(dialog.getAuthCookie());
        } else {

        }

        while (!shell.isDisposed()) {
            if (!display.readAndDispatch())
                display.sleep();
        }
        display.dispose();
    }

}
