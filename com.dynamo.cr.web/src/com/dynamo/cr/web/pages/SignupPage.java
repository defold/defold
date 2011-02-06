package com.dynamo.cr.web.pages;

import javax.ws.rs.core.MediaType;

import org.apache.wicket.feedback.FeedbackMessage;
import org.apache.wicket.feedback.IFeedbackMessageFilter;
import org.apache.wicket.markup.html.form.PasswordTextField;
import org.apache.wicket.markup.html.form.StatelessForm;
import org.apache.wicket.markup.html.form.TextField;
import org.apache.wicket.markup.html.panel.FeedbackPanel;
import org.apache.wicket.model.PropertyModel;
import org.apache.wicket.util.value.ValueMap;
import org.apache.wicket.validation.validator.EmailAddressValidator;

import com.dynamo.cr.protocol.proto.Protocol.RegisterUser;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.web.Session;
import com.dynamo.cr.web.panel.FormComponentFeedbackMessage;
import com.dynamo.cr.web.providers.ProtobufProviders;
import com.sun.jersey.api.client.ClientResponse;
import com.sun.jersey.api.client.WebResource;

public class SignupPage extends BasePage {

    private TextField<String> username;
    private TextField<String> firstName;
    private TextField<String> lastName;
    private PasswordTextField password;
    private PasswordTextField confirmPassword;

    public final class SignupForm extends StatelessForm<Void>
    {
        private static final long serialVersionUID = 1L;

        private final ValueMap properties = new ValueMap();

        public SignupForm(final String id)
        {
            super(id);

            username = new TextField<String>("username", new PropertyModel<String>(properties, "username"));
            username.add(EmailAddressValidator.getInstance());

            firstName = new TextField<String>("firstName", new PropertyModel<String>(properties, "firstName"));

            lastName = new TextField<String>("lastName", new PropertyModel<String>(properties, "lastName"));

            password = new PasswordTextField("password", new PropertyModel<String>(properties, "password"));
            confirmPassword = new PasswordTextField("confirmPassword", new PropertyModel<String>(properties, "confirmPassword"));

            FormComponentFeedbackMessage usernameBorder = new FormComponentFeedbackMessage("usernameBorder", username);
            add(usernameBorder);
            usernameBorder.add(username);

            FormComponentFeedbackMessage firstNameBorder = new FormComponentFeedbackMessage("firstNameBorder", firstName);
            add(firstNameBorder);
            firstNameBorder.add(firstName);

            FormComponentFeedbackMessage lastNameBorder = new FormComponentFeedbackMessage("lastNameBorder", lastName);
            add(lastNameBorder);
            lastNameBorder.add(lastName);

            FormComponentFeedbackMessage passwordBorder = new FormComponentFeedbackMessage("passwordBorder", password);
            add(passwordBorder);
            passwordBorder.add(password);

            FormComponentFeedbackMessage confirmPasswordBorder = new FormComponentFeedbackMessage("confirmPasswordBorder", confirmPassword);
            add(confirmPasswordBorder);
            confirmPasswordBorder.add(confirmPassword);

            username.setType(String.class);
            password.setType(String.class);
            confirmPassword.setType(String.class);
        }

        /**
         * @see org.apache.wicket.markup.html.form.Form#onSubmit()
         */
        @Override
        public final void onSubmit()
        {
            String passwordString = password.getDefaultModelObjectAsString();
            String confirmPasswordString = confirmPassword.getDefaultModelObjectAsString();

            if (passwordString.length() < 6) {
                password.error("Password is shorter than the minimum of 6 characters");
            }
            else if (!passwordString.equals(confirmPasswordString)) {
                password.error("Password doesn't match confirmed password");
            }
            else {
                Session session = (Session) getSession();
                WebResource adminWebResource = session.getAdminWebResource();

                String email = username.getDefaultModelObjectAsString();
                ClientResponse response = adminWebResource
                    .path("users/" + email)
                    .accept(MediaType.APPLICATION_JSON_TYPE)
                    .get(ClientResponse.class);

                if (response.getStatus() == 200) {
                    username.error(String.format("User with email %s already registred", email));
                    return;
                }
                else if (response.getStatus() != 404) {
                    throw new RuntimeException(String.format("Unexpected status code: %d", response.getStatus()));
                }

                RegisterUser registerUser = RegisterUser.newBuilder()
                        .setEmail(email)
                        .setFirstName(firstName.getDefaultModelObjectAsString())
                        .setLastName(lastName.getDefaultModelObjectAsString())
                        .setPassword(passwordString).build();

                adminWebResource
                    .path("users")
                    .accept(ProtobufProviders.APPLICATION_XPROTOBUF)
                    .type(ProtobufProviders.APPLICATION_XPROTOBUF)
                    .post(UserInfo.class, registerUser);

                if (!continueToOriginalDestination())
                {
                    setResponsePage(new WelcomeNewPage(firstName.getDefaultModelObjectAsString(), lastName.getDefaultModelObjectAsString()));
                }
            }
        }
    }

    public SignupPage() {
        final FeedbackPanel feedback = new FeedbackPanel("feedback");
        feedback.setFilter(new IFeedbackMessageFilter() {
            private static final long serialVersionUID = 1L;

            @Override
            public boolean accept(FeedbackMessage msg) {
                // Only show messages related to form
                // Form-component messages are handled by FormComponentFeedbackMessage
                return msg.getReporter().getClass() == SignupForm.class;
            }
        });

        add(feedback);
        add(new SignupForm("signupForm"));
    }

    public String getUsername() {
        return username.getDefaultModelObjectAsString();
    }

    public String getPassword() {
        return password.getDefaultModelObjectAsString();
    }

    public String getConfirmPassword() {
        return confirmPassword.getDefaultModelObjectAsString();
    }

    public String getFirstName() {
        return firstName.getDefaultModelObjectAsString();
    }

    public String getLastName() {
        return lastName.getDefaultModelObjectAsString();
    }

}
