package com.defold.extender;

import java.io.IOException;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;


@SpringBootApplication
public class ExtenderApplication {
    public static void main(String[] args) throws IOException, InterruptedException {
        SpringApplication.run(ExtenderApplication.class, args);
    }
}

