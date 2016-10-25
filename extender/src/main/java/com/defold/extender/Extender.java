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
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
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

        if (this.platformConfig == null) {
            throw new IllegalArgumentException(String.format("Unsupported platform %s", platform));
        }
    }

    private String[] substCmd(String template, Map<String, Object> context) {
        String s = Mustache.compiler().compile(template).execute(context);
        return s.split(" ");
    }

    private String subst(String template, Map<String, Object> context) {
        String s = Mustache.compiler().compile(template).execute(context);
        return s;
    }

    private List<String> subst(List<String> template, Map<String, Object> context) {
        List<String> ret = new ArrayList<>();
        for (String t : template) {
            ret.add(Mustache.compiler().compile(t).execute(context));
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

    private List<String> collectLibraries(File libDir, String re) {
        Pattern p = Pattern.compile(re);
        List<String> libs = new ArrayList<>();
        if (libDir.exists()) {
            File[] files = libDir.listFiles();
            for (File f : files) {
                Matcher m = p.matcher(f.getName());
                if (m.matches()) {
                    libs.add(m.group(1));
                }

            }
        }
        return libs;
    }

    private File linkEngine(List<File> extDirs, List<String> symbols) throws IOException, InterruptedException {
        File maincpp = new File(build, "main.cpp");
        File exe = new File(build, String.format("dmengine%s", platformConfig.exeExt));

        List<String> extSymbols = new ArrayList<>();
        extSymbols.addAll(symbols);

        Map<String, Object> mainContext = context();
        mainContext.put("ext", ImmutableMap.of("symbols", extSymbols));

        Template mainTemplate = Mustache.compiler().compile(config.main);
        String main = mainTemplate.execute(mainContext);
        FileUtils.writeStringToFile(maincpp, main);

        List<String> extLibs = new ArrayList<>();
        List<String> extLibPaths = new ArrayList<>(Arrays.asList(build.toString()));

        for (File ed : extDirs) {
            File libDir = new File(ed, "lib" + File.separator + this.platform);
            extLibs.addAll(collectLibraries(libDir, platformConfig.shlibRe));
            extLibs.addAll(collectLibraries(libDir, platformConfig.stlibRe));

            extLibPaths.add(new File(ed, "lib" + File.separator + this.platform).toString());
        }
        extLibs.addAll(collectLibraries(build, platformConfig.stlibRe));

        Map<String, Object> context = context();
        context.put("src", Arrays.asList(maincpp.getAbsolutePath()));
        context.put("tgt", exe.getAbsolutePath());
        context.put("ext", ImmutableMap.of("libs", extLibs, "libPaths", extLibPaths));

        String[] args = substCmd(platformConfig.linkCmd, context);
        exec(args);

        return exe;
    }

    private File compileFile(int i, File extDir, File f) throws IOException, InterruptedException {
        List<String> includes = new ArrayList<>();
        includes.add(extDir.getAbsolutePath() + File.separator + "include");
        File o = new File(build, String.format("%s_%d.o", f.getName(), i));

        Map<String, Object> context = context();
        context.put("src", f);
        context.put("tgt", o);
        context.put("ext", ImmutableMap.of("includes", includes));
        String[] args = substCmd(platformConfig.compileCmd, context);
        exec(args);
        return o;
    }

    @SuppressWarnings("unchecked")
    private Map<String, Object> context() {
        Map<String, Object> c = new HashMap<>(config.context);
        c.put("platform", this.platform);
        c.putAll(platformConfig.context);

        Set<String> keys = c.keySet();
        for (String k : keys) {
            Object v = c.get(k);
            if (v instanceof String ) {
                v = subst((String) v, c);
            } else if (v instanceof List) {
                v = subst((List<String>) v, c);
            }
            c.put(k, v);
        }

        return c;
    }

    private File buildExtension(File manifest) throws IOException, InterruptedException {
        File extDir = manifest.getParentFile();
        File src = new File(extDir, "src");
        Collection<File> srcFiles = new ArrayList<>();
        if (src.isDirectory()) {
            srcFiles = FileUtils.listFiles(src, null, true);
        }
        List<String> objs = new ArrayList<>();

        int i = 0;
        for (File f : srcFiles) {
            File o = compileFile(i, extDir, f);
            objs.add(o.getAbsolutePath());
            i++;
        }

        File lib = File.createTempFile("lib", ".a", build);
        lib.delete();

        Map<String, Object> context = context();
        context.put("tgt", lib);
        context.put("objs", objs);
        String[] args = substCmd(platformConfig.libCmd, context);
        exec(args);
        return lib;
    }

    public File buildEngine() throws IOException, InterruptedException {
        Collection<File> allFiles = FileUtils.listFiles(root, null, true);
        List<File> manifests = allFiles.stream().filter(f -> f.getName().equals("ext.manifest")).collect(Collectors.toList());
        List<File> extDirs = new ArrayList<>();
        for (File f : manifests) {
            extDirs.add(f.getParentFile());
        }

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

        return linkEngine(extDirs, symbols);
    }

    public void dispose() throws IOException {
        FileUtils.deleteDirectory(build);
    }

}
