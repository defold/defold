package com.dynamo.cr.web.panel;

/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import org.apache.wicket.authentication.AuthenticatedWebSession;
import org.apache.wicket.feedback.FeedbackMessage;
import org.apache.wicket.feedback.IFeedbackMessageFilter;
import org.apache.wicket.markup.html.WebMarkupContainer;
import org.apache.wicket.markup.html.form.CheckBox;
import org.apache.wicket.markup.html.form.PasswordTextField;
import org.apache.wicket.markup.html.form.StatelessForm;
import org.apache.wicket.markup.html.form.TextField;
import org.apache.wicket.markup.html.panel.FeedbackPanel;
import org.apache.wicket.markup.html.panel.Panel;
import org.apache.wicket.model.PropertyModel;
import org.apache.wicket.util.value.ValueMap;
import org.apache.wicket.validation.validator.EmailAddressValidator;


/**
 * Reusable user sign in panel with username and password as well as support for cookie persistence
 * of the both. When the LoginPanel's form is submitted, the method login(String, String) is
 * called, passing the username and password submitted. The login() method should authenticate the
 * user's session. The default implementation calls AuthenticatedWebSession.get().signIn().
 *
 * NOTE: Based on original wicket SigninPanel
 *
 * @auther Christian Murray
 * @author Jonathan Locke
 * @author Juergen Donnerstag
 * @author Eelco Hillenius
 */
public class LoginPanel extends Panel
{
    private static final long serialVersionUID = 1L;

    /** True if the panel should display a remember-me checkbox */
    private boolean includeRememberMe = true;

    /** Field for password. */
    private PasswordTextField password;

    /** True if the user should be remembered via form persistence (cookies) */
    private boolean rememberMe = true;

    /** Field for user name. */
    private TextField<String> username;

    /**
     * Sign in form.
     */
    public final class LoginForm extends StatelessForm<Void>
    {
        private static final long serialVersionUID = 1L;

        /** El-cheapo model for form. */
        private final ValueMap properties = new ValueMap();

        /**
         * Constructor.
         *
         * @param id
         *            id of the form component
         */
        public LoginForm(final String id)
        {
            super(id);

            // Attach textfield components that edit properties map
            // in lieu of a formal beans model
            username = new TextField<String>("username", new PropertyModel<String>(properties, "username"));
            username.add(EmailAddressValidator.getInstance());
            password = new PasswordTextField("password", new PropertyModel<String>(properties, "password"));
            FormComponentFeedbackMessage usernameBorder = new FormComponentFeedbackMessage("usernameBorder", username);
            add(usernameBorder);
            usernameBorder.add(username);

            FormComponentFeedbackMessage passwordBorder = new FormComponentFeedbackMessage("passwordBorder", password);
            add(passwordBorder);
            passwordBorder.add(password);

            username.setType(String.class);
            password.setType(String.class);

            // MarkupContainer row for remember me checkbox
            final WebMarkupContainer rememberMeRow = new WebMarkupContainer("rememberMeRow");
            add(rememberMeRow);

            // Add rememberMe checkbox
            rememberMeRow.add(new CheckBox("rememberMe", new PropertyModel<Boolean>(
                LoginPanel.this, "rememberMe")));

            // Make form values persistent
            setPersistent(rememberMe);

            // Show remember me checkbox?
            rememberMeRow.setVisible(includeRememberMe);
        }

        /**
         * @see org.apache.wicket.markup.html.form.Form#onSubmit()
         */
        @Override
        public final void onSubmit()
        {
            if (login(getUsername(), getPassword()))
            {
                onLoginSucceeded();
            }
            else
            {
                onLoginFailed();
            }
        }
    }

    /**
     * @see org.apache.wicket.Component#Component(String)
     */
    public LoginPanel(final String id)
    {
        this(id, true);
    }

    /**
     * @param id
     *            See Component constructor
     * @param includeRememberMe
     *            True if form should include a remember-me checkbox
     * @see org.apache.wicket.Component#Component(String)
     */
    public LoginPanel(final String id, final boolean includeRememberMe)
    {
        super(id);

        this.includeRememberMe = includeRememberMe;

        // Create feedback panel and add to page
        final FeedbackPanel feedback = new FeedbackPanel("feedback");
        add(feedback);
        feedback.setFilter(new IFeedbackMessageFilter() {
            private static final long serialVersionUID = 1L;

            @Override
            public boolean accept(FeedbackMessage msg) {
                // Only show messages related to form
                // Form-component messages are handled by FormComponentFeedbackMessage
                return msg.getReporter() == LoginPanel.this;
            }
        });

        // Add sign-in form to page, passing feedback panel as
        // validation error handler
        add(new LoginForm("loginForm"));
    }

    /**
     * Removes persisted form data for the login panel (forget me)
     */
    public final void forgetMe()
    {
        // Remove persisted user data. Search for child component
        // of type LoginForm and remove its related persistence values.
        getPage().removePersistedFormData(LoginPanel.LoginForm.class, true);
    }

    /**
     * Convenience method to access the password.
     *
     * @return The password
     */
    public String getPassword()
    {
        return password.getInput();
    }

    /**
     * Get model object of the rememberMe checkbox
     *
     * @return True if user should be remembered in the future
     */
    public boolean getRememberMe()
    {
        return rememberMe;
    }

    /**
     * Convenience method to access the username.
     *
     * @return The user name
     */
    public String getUsername()
    {
        return username.getDefaultModelObjectAsString();
    }

    /**
     * Convenience method set persistence for username and password.
     *
     * @param enable
     *            Whether the fields should be persistent
     */
    public void setPersistent(final boolean enable)
    {
        username.setPersistent(enable);
    }

    /**
     * Set model object for rememberMe checkbox
     *
     * @param rememberMe
     */
    public void setRememberMe(final boolean rememberMe)
    {
        this.rememberMe = rememberMe;
        setPersistent(rememberMe);
    }

    /**
     * Sign in user if possible.
     *
     * @param username
     *            The username
     * @param password
     *            The password
     * @return True if login was successful
     */
    public boolean login(String username, String password)
    {
        return AuthenticatedWebSession.get().signIn(username, password);
    }

    protected void onLoginFailed()
    {
        // Try the component based localizer first. If not found try the
        // application localizer. Else use the default
        error(getLocalizer().getString("signInFailed", this, "Sign in failed"));
    }

    protected void onLoginSucceeded()
    {
        // If login has been called because the user was not yet
        // logged in, than continue to the original destination,
        // otherwise to the Home page
        if (!continueToOriginalDestination())
        {
            setResponsePage(getApplication().getHomePage());
        }
    }
}