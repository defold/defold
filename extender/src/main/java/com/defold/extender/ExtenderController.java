package com.defold.extender;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;

import javax.servlet.http.HttpServletResponse;

import org.apache.commons.io.FileUtils;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestMethod;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.multipart.MultipartFile;
import org.springframework.web.multipart.MultipartHttpServletRequest;
import org.yaml.snakeyaml.Yaml;

@RestController
public class ExtenderController {

    private static String exec(String...args) throws IOException, InterruptedException {
        System.out.println(Arrays.toString(args) + " ...");
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

    private File buildEngine(List<File> libs, List<String> symbols) throws IOException, InterruptedException {
        ClassLoader cl = getClass().getClassLoader();
        File maincpp = File.createTempFile("main", ".cpp");

        File exportedcpp = File.createTempFile("exported_symbols", ".cpp");
        exportedcpp.deleteOnExit();

        File exe = File.createTempFile("dmengine", "");

        FileUtils.copyURLToFile(cl.getResource("main.cpp"), maincpp);

        List<String> allSymbols = new ArrayList<>();
        allSymbols.addAll(symbols);
        allSymbols.addAll(Arrays.asList("DefaultSoundDevice", "NullSoundDevice", "AudioDecoderWav", "CrashExt", "AudioDecoderStbVorbis",  "AudioDecoderTremolo"));

        StringBuilder sb = new StringBuilder();
        for (String s : allSymbols) {
            sb.append(String.format("extern \"C\" void %s();\n", s));
        }

        sb.append("void dmExportedSymbols() {\n");

        for (String s : allSymbols) {
            sb.append(String.format("    %s();\n", s));
        }

        sb.append("}\n");

        FileUtils.writeStringToFile(exportedcpp, sb.toString());

        String libPathHack = "-L/Users/chmu/Downloads/new3/sdk/redistributable_bin/osx32";
        String libHack = "-lsteam_api";

        List<String> args = Arrays.asList("clang++", libPathHack, libHack, maincpp.getAbsolutePath(), exportedcpp.getAbsolutePath(), "-o", exe.getAbsolutePath(), "-framework", "Cocoa", "-framework", "OpenGL", "-framework", "OpenAL", "-framework", "AGL", "-framework", "IOKit", "-framework", "Carbon", "-framework", "CoreVideo", "-framework", "Foundation", "-framework", "AppKit", "-m32", "-stdlib=libstdc++", "-mmacosx-version-min=10.7", "-framework", "Carbon", "-L/Users/chmu/tmp/dynamo-home/lib/darwin", "-L/Users/chmu/tmp/dynamo-home/ext/lib/darwin", "-lengine", "-ladtruthext", "-lfacebookext", "-liapext", "-lpushext", "-liacext", "-lrecord", "-lgameobject", "-lddf", "-lresource", "-lgamesys", "-lgraphics", "-lphysics", "-lBulletDynamics", "-lBulletCollision", "-lLinearMath", "-lBox2D", "-lrender", "-lscript", "-lluajit-5.1", "-lextension", "-lhid", "-linput", "-lparticle", "-ldlib", "-ldmglfw", "-lgui", "-ltracking", "-lcrashext", "-lsound2", "-ltremolo", "-lvpx");
        args = new ArrayList<>(args);

        for (File f : libs) {
            args.add(f.getAbsolutePath());
        }

        exec(args.toArray(new String[args.size()]));

        maincpp.delete();
        exportedcpp.delete();

        return exe;
    }

    private File compileFile(int i, File f, File build) throws IOException, InterruptedException {
        List<String> headers = new ArrayList<>(Arrays.asList("/Users/chmu/tmp/dynamo-home/include", "/Users/chmu/tmp/dynamo-home/ext/include"));
        headers.add(build.getParent() + "/include"); // TODO: HACK? :-)

        File o = new File(build, String.format("%s_%d.o", f.getName(), i));
        ArrayList<String> args = new ArrayList<>(Arrays.asList("clang++", "-c", "-m32", f.getAbsolutePath(), "-o", o.getAbsolutePath()));
        for (String h : headers) {
            args.add("-I" + h);
        }
        exec(args.toArray(new String[args.size()]));
        return o;
    }

    private File buildExtension(File manifest) throws IOException, InterruptedException {
        File root = manifest.getParentFile();
        File build = new File(root, "build");
        build.mkdirs();
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

        File lib = new File(build, "libX.a");

        List<String> args = new ArrayList<>();
        args.add("ar");
        args.add("rcs");
        args.add(lib.getAbsolutePath());
        args.addAll(objs);
        exec(args.toArray(new String[args.size()]));

        return lib;
    }

    private File buildEngine(List<MultipartFile> files, File root) throws IOException, InterruptedException {
        for (MultipartFile mpf : files) {
            File f = new File(root, mpf.getName());
            f.getParentFile().mkdirs();
            FileUtils.copyInputStreamToFile(mpf.getInputStream(), f);
        }

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

        return buildEngine(libs, symbols);

        //buildEngine(files);
    }

    @RequestMapping("/")
    public String index() {
        return "Extender";
    }

    @RequestMapping(method = RequestMethod.POST, value = "/build")
    public void build(MultipartHttpServletRequest req, HttpServletResponse resp) throws IOException, InterruptedException {
        Set<String> keys = req.getMultiFileMap().keySet();

        List<MultipartFile> files = new ArrayList<>();
        for (String k : keys) {
            MultipartFile f = req.getMultiFileMap().getFirst(k);
            files.add(f);
        }

        File root = Files.createTempDirectory("engine").toFile();
        File exe = buildEngine(files, root);

        FileUtils.copyFile(exe, resp.getOutputStream());
        exe.delete();

        FileUtils.deleteDirectory(root);
    }

}

