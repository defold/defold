# Defold Server


## Eclipse/Gradle

For Eclipse integration with gradle install gradle tools from the update-site
http://dist.springsource.com/release/TOOLS/gradle


## Build, Deploy, Test and Run

Run unit-tests

    ./gradlew test

Build distribution

    ./gradlew clean distZip

Deploy and unzip `build/distributions/server-1.0.zip`

Run with

    JAVA_OPTS=-Dserver.config=MY.CONFIG ./bin/server


## Misc

If you get an error similar to this or why we can't user über jar

    2015-04-02 13:54:49,306 ERROR [Grizzly(1)] c.s.j.s.c.ContainerResponse [ContainerResponse.java:276]
    The registered message body writers compatible with the MIME media type are:
    */* ->
    com.sun.jersey.server.impl.template.ViewableMessageBodyWriter


See http://blog.dejavu.sk/2015/03/24/what-to-do-when-jax-rs-cannot-find-its-providers-aka-my-message-body-writer-is-not-used/

*The reason is you’re creating your über jar using mis-configured assembly plugin (e.g. from Maven) that, by default, handles META-INF/services incorrectly. Files in META-INF/services are not CONCATENATED as they should be, they’re overwritten by each other. This means that if you have same services (e.g. MessageBodyWriter) in multiple Jars then only services from one (the last one) Jar are registered.*
