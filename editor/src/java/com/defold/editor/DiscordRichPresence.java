// Copyright 2020-2025 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.defold.editor;

import com.google.gson.JsonObject;
import com.jagrosh.discordipc.IPCClient;
import com.jagrosh.discordipc.IPCListener;
import com.jagrosh.discordipc.entities.ActivityType;
import com.jagrosh.discordipc.entities.Packet;
import com.jagrosh.discordipc.entities.RichPresence;
import com.jagrosh.discordipc.entities.User;
import com.jagrosh.discordipc.exceptions.NoDiscordClientException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.time.OffsetDateTime;

final class DiscordRichPresence {

    private static final Logger logger = LoggerFactory.getLogger(DiscordRichPresence.class);
    private static final long CLIENT_ID = 1450057060514402396L;
    public static final String STATE = "Making a game";

    static void startAsync() {
        Thread.ofVirtual()
                .name("discord-rich-presence")
                .start(DiscordRichPresence::connect);
    }

    private static void connect() {
        IPCClient client = new IPCClient(CLIENT_ID);
        client.setListener(new IPCListener() {
            @Override
            public void onPacketSent(IPCClient ipcClient, Packet packet) {
            }

            @Override
            public void onPacketReceived(IPCClient ipcClient, Packet packet) {
            }

            @Override
            public void onActivityJoin(IPCClient ipcClient, String s) {
            }

            @Override
            public void onActivitySpectate(IPCClient ipcClient, String s) {
            }

            @Override
            public void onActivityJoinRequest(IPCClient ipcClient, String s, User user) {
            }

            @Override
            public void onReady(IPCClient client) {
                RichPresence.Builder builder = new RichPresence.Builder();
                builder.setState(STATE)
                        .setActivityType(ActivityType.Playing)
                        .setStartTimestamp(OffsetDateTime.now().toEpochSecond());
                client.sendRichPresence(builder.build());
            }

            @Override
            public void onClose(IPCClient ipcClient, JsonObject jsonObject) {
            }

            @Override
            public void onDisconnect(IPCClient ipcClient, Throwable throwable) {
            }
        });

        try {
            client.connect();
        } catch (NoDiscordClientException e) {
            logger.info("Discord connection: {}", e.getMessage());
        } catch (Throwable t) {
            logger.warn("Discord connection failed: {}", t.toString());
            logger.debug("Discord connection failure details", t);
        }
    }
}
