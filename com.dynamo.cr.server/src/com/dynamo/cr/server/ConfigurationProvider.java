package com.dynamo.cr.server;

import java.io.FileReader;
import java.io.IOException;

import javax.inject.Inject;
import javax.inject.Named;
import javax.inject.Provider;

import com.dynamo.cr.proto.Config;
import com.dynamo.cr.proto.Config.Configuration;
import com.google.protobuf.TextFormat;

public class ConfigurationProvider implements Provider<Configuration>{

    private String configurationFile;

    @Inject
    public ConfigurationProvider(@Named("configurationFile") String configurationFile) {
        this.configurationFile = configurationFile;
    }

    @Override
    public Configuration get() {
        try {
            FileReader fr = new FileReader(configurationFile);
            Config.Configuration.Builder builder = Config.Configuration.newBuilder();
            TextFormat.merge(fr, builder);
            return builder.build();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

}
