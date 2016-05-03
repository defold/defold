package com.defold.extender;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

import org.apache.commons.io.FileUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.yaml.snakeyaml.Yaml;

import com.google.common.collect.ImmutableMap;
import com.samskivert.mustache.Mustache;
import com.samskivert.mustache.Template;


public class Extender {

    private Configuration config;
    private String platform;
    private File root;
    private File build;
    private PlatformConfig platformConfig;
    private static Logger logger = LoggerFactory.getLogger(Extender.class);

    public Extender(Configuration config, String platform, File root) throws IOException {
        this.config = config;
        this.platform = platform;
        this.platformConfig = config.platforms.get(platform);
        this.root = root;
        this.build = Files.createTempDirectory("engine").toFile();
    }

    private String[] subst(String template, Map<String, Object> context) {
        String s = Mustache.compiler().compile(template).execute(context);
        return s.split(" ");
    }

    private String[] subst(String[] template, Map<String, Object> context) {
        String[] ret = new String[template.length];
        for (int i = 0; i < template.length; i++) {
            ret[i] = Mustache.compiler().compile(template[i]).execute(context);
        }
        return ret;
    }

    private static String exec(String...args) throws IOException, InterruptedException {
        logger.info(Arrays.toString(args));

        ProcessBuilder pb = new ProcessBuilder(args);
        pb.redirectErrorStream(true);
        Process p = pb.start();

        int ret = 127;
        byte[] buf = new byte[16 * 1024];
        InputStream is = p.getInputStream();

        StringBuilder sb = new StringBuilder();

        int n = 0;
        do {
            n = is.read(buf);
            if (n > 0) {
                sb.append(new String(buf, 0, n));
            }
        }
        while (n > 0);
        ret = p.waitFor();
        if (ret > 0) {
            throw new IOException(sb.toString());
        }

        return sb.toString();
    }

    private File linkEngine(List<File> libs, List<String> symbols) throws IOException, InterruptedException {
        File maincpp = new File(build, "main.cpp");
        File exe = new File(build, String.format("dmengine%s", platformConfig.exeExt));

        List<String> allSymbols = new ArrayList<>();
        allSymbols.addAll(symbols);
        allSymbols.addAll(Arrays.asList(config.exportedSymbols));

        Template mainTemplate = Mustache.compiler().compile(config.main);
        String main = mainTemplate.execute(ImmutableMap.of("symbols", allSymbols));
        FileUtils.writeStringToFile(maincpp, main);

        Map<String, Object> context = context();
        context.put("src", Arrays.asList(maincpp.getAbsolutePath()));
        context.put("tgt", exe.getAbsolutePath());
        context.put("extLibs", libs);

        String[] args = subst(platformConfig.linkCmd, context);
        exec(args);

        return exe;
    }

    private File compileFile(int i, File f, File build) throws IOException, InterruptedException {
        Map<String, Object> context = context();
        List<String> includes = new ArrayList<>(Arrays.asList(subst(config.includes, context)));
        includes.add(root.getAbsolutePath() + File.separator + "include");
        File o = new File(build, String.format("%s_%d.o", f.getName(), i));
        String[] args = subst(platformConfig.compileCmd, ImmutableMap.of("src", f, "tgt", o, "includes", includes));
        exec(args);
        return o;
    }

    private Map<String, Object> context() {
        Map<String, Object> c = new HashMap<>(config.context);
        c.putAll(platformConfig.context);
        return c;
    }

    private File buildExtension(File manifest) throws IOException, InterruptedException {
        File root = manifest.getParentFile();
        File src = new File(root, "src");
        Collection<File> srcFiles = new ArrayList<>();
        if (src.isDirectory()) {
            srcFiles = FileUtils.listFiles(src, null, true);
        }
        List<String> objs = new ArrayList<>();

        int i = 0;
        for (File f : srcFiles) {
            File o = compileFile(i, f, build);
            objs.add(o.getAbsolutePath());
            i++;
        }

        File lib = File.createTempFile("lib", ".a", build);
        lib.delete();

        String[] args = subst(platformConfig.libCmd, ImmutableMap.of("tgt", lib, "objs", objs));
        exec(args);
        return lib;
    }

    public File buildEngine() throws IOException, InterruptedException {
        Collection<File> allFiles = FileUtils.listFiles(root, null, true);
        List<File> manifests = allFiles.stream().filter(f -> f.getName().equals("ext.manifest")).collect(Collectors.toList());

        List<String> symbols = new ArrayList<>();

        for (File f : manifests) {
            @SuppressWarnings("unchecked")
            Map<String, Object> m = (Map<String, Object>) new Yaml().load(FileUtils.readFileToString(f));
            symbols.add((String) m.get("name"));
        }

        List<File> libs = new ArrayList<>();
        for (File manifest : manifests) {
            File lib = buildExtension(manifest);
            libs.add(lib);
        }

        return linkEngine(libs, symbols);
    }

    public void dispose() throws IOException {
        FileUtils.deleteDirectory(build);
    }

}
