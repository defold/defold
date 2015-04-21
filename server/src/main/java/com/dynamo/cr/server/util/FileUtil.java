package com.dynamo.cr.server.util;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;

public class FileUtil {

    public static String readEntireFile(File file) throws IOException {
        BufferedReader br = new BufferedReader(new FileReader(file));
        final char [] chars = new char[(int) file.length()];
        br.read(chars);
        br.close();
        return new String(chars);
    }

    public static void removeDir(File dir) throws IOException {
        if (!dir.exists())
            return;

        String[] list = dir.list();
        for (int i = 0; i < list.length; i++) {
            String s = list[i];
            File f = new File(dir, s);
            if (f.isDirectory()) {
                removeDir(f);
            }
            else if (!f.delete()) throw new RuntimeException();

        }

        if (!dir.delete())
            throw new RuntimeException();
    }

}
