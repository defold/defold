package com.dynamo.cr.server.model;

import java.util.Date;

import javax.persistence.Column;
import javax.persistence.Entity;
import javax.persistence.GeneratedValue;
import javax.persistence.GenerationType;
import javax.persistence.Id;
import javax.persistence.Table;
import javax.persistence.Temporal;
import javax.persistence.TemporalType;

@Entity
@Table(name="jobs")
public class Job {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column
    private String queue;

    @Column(nullable = false)
    private byte[] data;

    @Temporal(TemporalType.TIMESTAMP)
    private Date earliest = new Date();

    @Column(nullable = false)
    int retries = 0;

    public Job() {
    }

    public Job(byte[] data, String queue) {
        this.data = data;
        this.queue = queue;
    }

    public Long getId() {
        return id;
    }

    public String getQueue() {
        return queue;
    }

    public byte[] getData() {
        return data;
    }

    public Date getEarliest() {
        return earliest;
    }

    public void setEarliest(Date earliest) {
        this.earliest = earliest;
    }

    public int getRetries() {
        return retries;
    }

    public void setRetries(int retries) {
        this.retries = retries;
    }

    @Override
    public String toString() {
        return String.format("job (%d, %s)", getId(), getEarliest());
    }
}
