package com.dynamo.cr.server.model;

import javax.persistence.Column;
import javax.persistence.Entity;
import javax.persistence.GeneratedValue;
import javax.persistence.GenerationType;
import javax.persistence.Id;
import javax.persistence.Table;

@Entity
@Table(name = "products")
public class Product {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false)
    private String name;

    @Column(nullable = false)
    private Integer maxMemberCount;

    @Column(nullable = false, unique = true)
    private String handle;

    @Column(nullable = false)
    private Boolean isDefault;

    @Column(nullable = false) private Integer fee = 0;

    public Long getId() {
        return id;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public Integer getMaxMemberCount() {
        return maxMemberCount;
    }

    public void setMaxMemberCount(Integer maxMemberCount) {
        this.maxMemberCount = maxMemberCount;
    }

    public String getHandle() {
        return handle;
    }

    public void setHandle(String handle) {
        this.handle = handle;
    }

    public Boolean isDefault() {
        return isDefault;
    }

    public void setDefault(Boolean isDefault) {
        this.isDefault = isDefault;
    }

    public Integer getFee() {
        return this.fee;
    }

    public void setFee(int fee) {
        this.fee = fee;
    }

    @Override
    public String toString() {
        return String.format("%s", this.name);
    }

}
