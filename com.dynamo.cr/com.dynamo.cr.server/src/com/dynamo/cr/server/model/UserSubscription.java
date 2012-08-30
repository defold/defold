package com.dynamo.cr.server.model;

import javax.persistence.Column;
import javax.persistence.Embeddable;
import javax.persistence.Embedded;
import javax.persistence.Entity;
import javax.persistence.EnumType;
import javax.persistence.Enumerated;
import javax.persistence.GeneratedValue;
import javax.persistence.GenerationType;
import javax.persistence.Id;
import javax.persistence.OneToOne;
import javax.persistence.Table;

@Entity
@Table(name = "user_subscriptions")
public class UserSubscription {

    @Embeddable
    public static class CreditCard {
        private String maskedNumber;
        private Integer expirationMonth;
        private Integer expirationYear;

        public CreditCard() {

        }

        public CreditCard(String maskedNumber, Integer expirationMonth, Integer expirationYear) {
            this.maskedNumber = maskedNumber;
            this.expirationMonth = expirationMonth;
            this.expirationYear = expirationYear;
        }

        public String getMaskedNumber() {
            return this.maskedNumber;
        }

        public void setMaskedNumber(String maskedNumber) {
            this.maskedNumber = maskedNumber;
        }

        public int getExpirationMonth() {
            return this.expirationMonth;
        }

        public void setExpirationMonth(int expirationMonth) {
            this.expirationMonth = expirationMonth;
        }

        public int getExpirationYear() {
            return this.expirationYear;
        }

        public void setExpirationYear(int expirationYear) {
            this.expirationYear = expirationYear;
        }
    }

    public enum State {
        CANCELED, PENDING, ACTIVE,
    }

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @OneToOne(optional = false)
    private User user;

    @Column(nullable = false)
    private Long productId;

    @Column(nullable = false, unique = true)
    private Long externalId;

    @Column(nullable = false, unique = true)
    private Long externalCustomerId;

    @Column(nullable = false)
    @Enumerated(EnumType.STRING)
    private State state = State.PENDING;

    @Column
    private String cancellationMessage;

    @Embedded
    private CreditCard creditCard;

    public Long getId() {
        return id;
    }

    public User getUser() {
        return user;
    }

    public void setUser(User user) {
        this.user = user;
    }

    public Long getProductId() {
        return productId;
    }

    public void setProductId(Long productId) {
        this.productId = productId;
    }

    public Long getExternalId() {
        return externalId;
    }

    public void setExternalId(Long externalId) {
        this.externalId = externalId;
    }

    public Long getExternalCustomerId() {
        return externalCustomerId;
    }

    public void setExternalCustomerId(Long externalCustomerId) {
        this.externalCustomerId = externalCustomerId;
    }

    public State getState() {
        return this.state;
    }

    public void setState(State state) {
        this.state = state;
    }

    public String getCancellationMessage() {
        return this.cancellationMessage;
    }

    public void setCancellationMessage(String cancellationMessage) {
        this.cancellationMessage = cancellationMessage;
    }

    public CreditCard getCreditCard() {
        return this.creditCard;
    }

    public void setCreditCard(CreditCard creditCard) {
        this.creditCard = creditCard;
    }

    @Override
    public String toString() {
        return "" + this.user + " - " + this.productId;
    }

}
